/*
 *  HRAW - Hacker's toolkit for image sensor characterisation
 *  Copyright 2016-2018 Ciriaco Garcia de Celis
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <cmath>
#include <numeric>
#include <array>
#include "Util.hpp"
#include "ImageAlgo.h"

struct ChannelIterators
{
    ChannelIterators(const std::shared_ptr<class RawImage>& image, bool unmasked = false)
      : red(image->getChannel(ImageFilter::R ())->select(unmasked)),
        gr1(image->getChannel(ImageFilter::G1())->select(unmasked)),
        gr2(image->getChannel(ImageFilter::G2())->select(unmasked)),
        blu(image->getChannel(ImageFilter::B ())->select(unmasked))
    {
    }

    inline explicit operator bool() const // return false if no more pixels
    {
        return red.operator bool(); // assumed all channels in sync (but this one gives you wings ;-)
    }

    inline void operator++(int) // jump to next pixels
    {
        red++;
        gr1++;
        gr2++;
        blu++;
    }

    ImageSelection::Iterator red;
    ImageSelection::Iterator gr1;
    ImageSelection::Iterator gr2;
    ImageSelection::Iterator blu;
};

struct ChannelBlacks
{
    ChannelBlacks(const ChannelIterators& rgb)
      : red(rgb.red.selection->channel->blackLevel()),
        gr1(rgb.gr1.selection->channel->blackLevel()),
        gr2(rgb.gr2.selection->channel->blackLevel()),
        blu(rgb.blu.selection->channel->blackLevel())
    {}

    const double red;
    const double gr1;
    const double gr2;
    const double blu;
};

void ImageAlgo::setBlackLevel(const RawImage::ptr& image, std::vector<double> blackPoints)
{
    if (blackPoints.empty() && image->masked.left) // if not externally supplied compute them from the masked pixels
    {
        blackPoints.emplace_back(ImageMath::analyze(image->getChannel(ImageFilter::R())->getLeftMask()).mean);
        blackPoints.emplace_back(ImageMath::analyze(image->getChannel(ImageFilter::G1())->getLeftMask()).mean);
        blackPoints.emplace_back(ImageMath::analyze(image->getChannel(ImageFilter::G2())->getLeftMask()).mean);
        blackPoints.emplace_back(ImageMath::analyze(image->getChannel(ImageFilter::B())->getLeftMask()).mean);
    }

    if (!blackPoints.empty())
    {
        auto& blacks = image->blackLevel;
        blacks.clear();

        if (blackPoints.size() == 4)
        {
            blacks.emplace(ImageFilter::Code::R,   blackPoints.at(0));
            blacks.emplace(ImageFilter::Code::G1,  blackPoints.at(1));
            blacks.emplace(ImageFilter::Code::G2,  blackPoints.at(2));
            blacks.emplace(ImageFilter::Code::B,   blackPoints.at(3));
            blacks.emplace(ImageFilter::Code::G,   (blackPoints.at(1) + blackPoints.at(2)) / 2);
            blacks.emplace(ImageFilter::Code::RGB, std::accumulate(blackPoints.begin(), blackPoints.end(), 0.0) / 4);
        }
        else
        {
            blacks.emplace(ImageFilter::Code::R,   blackPoints.at(0));
            blacks.emplace(ImageFilter::Code::G1,  blackPoints.at(0));
            blacks.emplace(ImageFilter::Code::G2,  blackPoints.at(0));
            blacks.emplace(ImageFilter::Code::B,   blackPoints.at(0));
            blacks.emplace(ImageFilter::Code::G,   blackPoints.at(0));
            blacks.emplace(ImageFilter::Code::RGB, blackPoints.at(0));
        }
    }
}

void ImageAlgo::setWhiteLevel(const RawImage::ptr& image, std::shared_ptr<bitdepth_t> whitePoint)
{
    image->whiteLevel = whitePoint;
}

ImageAlgo::Levels ImageAlgo::autoLevels(const ImageMath::Histogram::ptr& histogram)
{
    if (histogram->data.empty()) throw ImageException("autoLevels: empty histogram");
    Levels info { 0, 0, 0 };
    bitdepth_t levels = 0;
    for (auto itf = histogram->data.cbegin(); itf != histogram->data.cend(); ++itf)
    {
        if (levels++ < 8) continue;
        info.blackLevel = bitdepth_t(pow(2, int(log(itf->first) / log(2) + 0.5)));
        break;
    }
    bitdepth_t suspiciousLevel = 0;
    imgsize_t spike = 1;
    imgsize_t clippedCount = 0;
    imgsize_t clipThreshold = 16;
    bitdepth_t level = bitdepth_t(histogram->data.crbegin()->first + 1);
    levels = 0;
    for (auto ritf = histogram->data.crbegin(); ritf != histogram->data.crend(); ++ritf)
    {
        if (levels++ > 128) break;
        level = ritf->first;
        imgsize_t pixels = ritf->second;
        clippedCount += pixels;
        if (!info.whiteLevel) info.whiteLevel = level;
        if (pixels > 4)
        {
            if ((pixels >= clipThreshold * (levels == 1? 1 : histogram->hDelta)) // account for compression
                && (pixels / spike >= clipThreshold)) // account for spurious data after clipping
            {
                suspiciousLevel = level;
                info.clippedCount = clippedCount;
            }
            if (pixels > spike) spike = pixels;
        }
    }
    if (suspiciousLevel) info.whiteLevel = suspiciousLevel; else info.clippedCount = 0;
    if (info.clippedCount)
    {
        auto prevclip = histogram->data.find(info.whiteLevel);
        if (prevclip != histogram->data.begin())
        {
            --prevclip;
            if (prevclip->second > info.clippedCount / 4) // some channel clipped in the previous level?
            {
                info.clippedCount += prevclip->second; // just in case
                info.whiteLevel = prevclip->first;
            }
        }
    }
    return info;
}

RawImage::ptr ImageAlgo::clipping(const RawImage::ptr& input) // assumed RGGB bayer geometry
{
    if (!input->hasBlackLevel()) throw ImageException("clipping: missing black point");
    if (!input->whiteLevel) throw ImageException("clipping: missing white point");
    double avgBlackLevel = input->blackLevel[ImageFilter::Code::RGB];
    bitdepth_t blackLevel = bitdepth_t(std::round(avgBlackLevel));
    bitdepth_t whiteLevel = *input->whiteLevel;
    ChannelIterators in(input, true);

    imgsize_t outputWidth = (input->rowPixels / 2 - input->masked.left) * 3;
    imgsize_t outputHeight = input->colPixels / 2 - input->masked.top;
    RawImage::ptr copy = RawImage::create(outputWidth, outputHeight, RawImage::Masked{ 0, 0 });
    ImageChannel::ptr plain = copy->getChannel(ImageFilter::RGB());
    ImageSelection::ptr full = plain->select();
    ImageSelection::Iterator out(full);

    bitdepth_t outclip = 65535; // 16-bit output
    double maxWhite = whiteLevel - avgBlackLevel;
    double brightnessAdjust = pow(2, log(outclip)/log(2) - 0.5) / maxWhite; // separate non clipped data 0.5EV

    auto gamma = [&maxWhite](double adu) -> double { return pow(adu / maxWhite, 1/2.2) * maxWhite; };

    auto glut = std::make_shared<std::array<double, 16384>>(); // fast lookup cache for the usual 14-bit ADC data
    auto& fastgamma = *glut;
    for (bitdepth_t adu = 0; adu < fastgamma.size(); adu++) fastgamma[adu] = gamma(adu);

    while (out)
    {
        bitdepth_t red = in.red++;
        bitdepth_t gr1 = in.gr1++;
        bitdepth_t gr2 = in.gr2++;
        bitdepth_t blu = in.blu++;

        if ((red >= whiteLevel) || (gr1 >= whiteLevel) || (gr2 >= whiteLevel) || (blu >= whiteLevel)) // any burnt subpixel?
        {
            red = red >= whiteLevel? outclip : 0;
            gr1 = (gr1 >= whiteLevel) || (gr2 >= whiteLevel)? outclip : 0;
            blu = blu >= whiteLevel? outclip : 0;
        }
        else // cheap demosaicing (a quarter of the original resolution)
        {
            red = bitdepth_t(red > blackLevel ? red - blackLevel : 0); // beware of negative noise in the shadows
            gr1 = bitdepth_t(gr1 > blackLevel ? gr1 - blackLevel : 0);
            gr2 = bitdepth_t(gr2 > blackLevel ? gr2 - blackLevel : 0);
            blu = bitdepth_t(blu > blackLevel ? blu - blackLevel : 0);
            double bw = 0.299 * red + 0.587 * (gr1 + gr2) / 2 + 0.114 * blu; // convert to B&W
            bitdepth_t adu = bitdepth_t(bw);
            bw = adu < fastgamma.size()? fastgamma[adu] : gamma(bw); // gamma correction
            bw = bw * brightnessAdjust;
            red = gr1 = blu = bitdepth_t(bw);
        }

        out = red;
        out++;
        out = gr1;
        out++;
        out = blu;
        out++;
    }

    return copy;
}

RawImage::ptr ImageAlgo::dprawProcess(const DPRAW& dpraw, DPRAW::Action action, DPRAW::ProcessMode processMode)
{
    if (!dpraw.imgAB->sameSizeAs(dpraw.imgB))
        throw ImageException("dprawProcess: image and subimage size don't match");

    auto newImage = RawImage::layout(dpraw.imgAB);
    ChannelIterators inAB(dpraw.imgAB);
    ChannelIterators inB(dpraw.imgB);
    ChannelIterators out(newImage);

    ChannelBlacks blackAB(inAB);
    ChannelBlacks blackB(inB);

    auto white = dpraw.white;

    if (action == DPRAW::Action::GetA) // compute the A subframe subtracting B from AB
    {
        if (processMode == DPRAW::ProcessMode::Plain)
        {
            while (out)
            {
                out.red = inAB.red >= white? inB.red : 0.5 + (inAB.red - blackAB.red) - (inB.red - blackB.red) + blackB.red;
                out.gr1 = inAB.gr1 >= white? inB.gr1 : 0.5 + (inAB.gr1 - blackAB.gr1) - (inB.gr1 - blackB.gr1) + blackB.gr1;
                out.gr2 = inAB.gr2 >= white? inB.gr2 : 0.5 + (inAB.gr2 - blackAB.gr2) - (inB.gr2 - blackB.gr2) + blackB.gr2;
                out.blu = inAB.blu >= white? inB.blu : 0.5 + (inAB.blu - blackAB.blu) - (inB.blu - blackB.blu) + blackB.blu;
                out++;
                inAB++;
                inB++;
            }
        }
        else // "Bayer" mode
        {
            while (out)
            {
                if ((inAB.red < white) && (inAB.gr1 < white) && (inAB.gr2 < white) && (inAB.blu < white))
                {
                    out.red = 0.5 + (inAB.red++ - blackAB.red) - (inB.red++ - blackB.red) + blackB.red;
                    out.gr1 = 0.5 + (inAB.gr1++ - blackAB.gr1) - (inB.gr1++ - blackB.gr1) + blackB.gr1;
                    out.gr2 = 0.5 + (inAB.gr2++ - blackAB.gr2) - (inB.gr2++ - blackB.gr2) + blackB.gr2;
                    out.blu = 0.5 + (inAB.blu++ - blackAB.blu) - (inB.blu++ - blackB.blu) + blackB.blu;
                    out++;
                }
                else // any channel overexposed
                {
                    out.red = out.gr1 = out.gr2 = out.blu = white;
                    out++;
                    inAB++;
                    inB++;
                }
            }
        }
    }
    else if (action == DPRAW::Action::Blend) // replace AB overexposed areas with B, shifting to match the exposure
    {
        double scale = pow(2.0, *dpraw.shiftEV);

        if (processMode == DPRAW::ProcessMode::Plain)
        {
            while (out)
            {
                out.red = inAB.red >= white? inB.red : 0.5 + (inAB.red - blackAB.red) * scale + blackB.red;
                out.gr1 = inAB.gr1 >= white? inB.gr1 : 0.5 + (inAB.gr1 - blackAB.gr1) * scale + blackB.gr1;
                out.gr2 = inAB.gr2 >= white? inB.gr2 : 0.5 + (inAB.gr2 - blackAB.gr2) * scale + blackB.gr2;
                out.blu = inAB.blu >= white? inB.blu : 0.5 + (inAB.blu - blackAB.blu) * scale + blackB.blu;
                out++;
                inAB++;
                inB++;
            }
        }
        else // "Bayer" mode
        {
            while (out)
            {
                if ((inAB.red < white) && (inAB.gr1 < white) && (inAB.gr2 < white) && (inAB.blu < white))
                {
                    out.red = 0.5 + (inAB.red++ - blackAB.red) * scale + blackB.red;
                    out.gr1 = 0.5 + (inAB.gr1++ - blackAB.gr1) * scale + blackB.gr1;
                    out.gr2 = 0.5 + (inAB.gr2++ - blackAB.gr2) * scale + blackB.gr2;
                    out.blu = 0.5 + (inAB.blu++ - blackAB.blu) * scale + blackB.blu;
                    out++;
                    inB++;
                }
                else // any channel overexposed
                {
                    out.red = inB.red++;
                    out.gr1 = inB.gr1++;
                    out.gr2 = inB.gr2++;
                    out.blu = inB.blu++;
                    out++;
                    inAB++;
                }
            }
        }
    }

    return newImage;
}

std::istream& operator>>(std::istream& in, ImageAlgo::DPRAW::Action& action)
{
    std::string str;
    if (in >> str)
    {
        str = String::tolower(str);
             if (!str.compare("geta"))  action = ImageAlgo::DPRAW::Action::GetA;
        else if (!str.compare("blend")) action = ImageAlgo::DPRAW::Action::Blend;
        else in.setstate(std::ios_base::failbit);
    }
    return in;
}

std::istream& operator>>(std::istream& in, ImageAlgo::DPRAW::ProcessMode& processMode)
{
    std::string str;
    if (in >> str)
    {
        str = String::tolower(str);
             if (!str.compare("plain")) processMode = ImageAlgo::DPRAW::ProcessMode::Plain;
        else if (!str.compare("bayer")) processMode = ImageAlgo::DPRAW::ProcessMode::Bayer;
        else in.setstate(std::ios_base::failbit);
    }
    return in;
}
