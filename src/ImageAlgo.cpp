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

void ImageAlgo::setBlackLevel(const RawImage::ptr& image, std::shared_ptr<std::vector<double>> blackPoints)
{
    if (!blackPoints && image->masked.left) // if not externally supplied compute them from the masked pixels
    {
        blackPoints = std::make_shared<std::vector<double>>();
        blackPoints->emplace_back(ImageMath::analyze(image->getChannel(ImageFilter::R())->getLeftMask()).mean);
        blackPoints->emplace_back(ImageMath::analyze(image->getChannel(ImageFilter::G1())->getLeftMask()).mean);
        blackPoints->emplace_back(ImageMath::analyze(image->getChannel(ImageFilter::G2())->getLeftMask()).mean);
        blackPoints->emplace_back(ImageMath::analyze(image->getChannel(ImageFilter::B())->getLeftMask()).mean);
    }

    if (blackPoints && !blackPoints->empty())
    {
        auto& blacks = image->blackLevel;
        blacks.clear();

        if (blackPoints->size() == 4)
        {
            blacks.emplace(ImageFilter::Code::R,   blackPoints->at(0));
            blacks.emplace(ImageFilter::Code::G1,  blackPoints->at(1));
            blacks.emplace(ImageFilter::Code::G2,  blackPoints->at(2));
            blacks.emplace(ImageFilter::Code::B,   blackPoints->at(3));
            blacks.emplace(ImageFilter::Code::G,   (blackPoints->at(1) + blackPoints->at(2)) / 2);
            blacks.emplace(ImageFilter::Code::RGB, std::accumulate(blackPoints->begin(), blackPoints->end(), 0.0) / 4);
        }
        else
        {
            blacks.emplace(ImageFilter::Code::R,   blackPoints->at(0));
            blacks.emplace(ImageFilter::Code::G1,  blackPoints->at(0));
            blacks.emplace(ImageFilter::Code::G2,  blackPoints->at(0));
            blacks.emplace(ImageFilter::Code::B,   blackPoints->at(0));
            blacks.emplace(ImageFilter::Code::G,   blackPoints->at(0));
            blacks.emplace(ImageFilter::Code::RGB, blackPoints->at(0));
        }
    }
}

void ImageAlgo::setWhiteLevel(const RawImage::ptr& image, std::shared_ptr<bitdepth_t> whitePoint)
{
    image->whiteLevel = whitePoint;
}

ImageAlgo::Highlights ImageAlgo::getHighlights(const ImageMath::Histogram::ptr& histogram)
{
    if (histogram->data.empty()) throw ImageException("getHighlights: empty histogram");
    Highlights info { 0, 0 };
    info.clippedCount = 0;
    uint64_t threshold = histogram->total / 10000;
    for (auto ritf = histogram->data.crbegin(); ritf != histogram->data.crend(); ++ritf)
    {
        info.clippedCount += ritf->second;
        if (ritf->second > threshold)
        {
            if (info.clippedCount - ritf->second < threshold / 10) // overexposed
            {
                info.whiteLevel = ++ritf != histogram->data.rend()? ritf->first : 0;
            }
            else
            {
                info.clippedCount = 0;
                while ((ritf != histogram->data.crbegin()) && ritf->second) ++ritf;
                if (!ritf->second) --ritf;
                info.whiteLevel = ritf->first;
            }
            break;
        }
    }
    return info;
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
