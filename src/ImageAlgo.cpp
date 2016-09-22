/*
 *  HRAW - Hacker's toolkit for image sensor characterisation
 *  Copyright 2016 Ciriaco Garcia de Celis
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
#include "Util.hpp"
#include "ImageAlgo.h"

struct ChannelIterators
{
    ChannelIterators(const std::shared_ptr<class RawImage>& image)
      : red(image->getChannel(FilterPattern::RGGB_R ())),
        gr1(image->getChannel(FilterPattern::RGGB_G1())),
        gr2(image->getChannel(FilterPattern::RGGB_G2())),
        blu(image->getChannel(FilterPattern::RGGB_B ()))
    {}

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

ImageAlgo::Highlights ImageAlgo::getHighlights(const ImageMath::Histogram& histogram)
{
    if (histogram.data.empty()) throw ImageException("getHighlights: empty histogram");
    Highlights info { 0, 0 };
    info.clippedCount = 0;
    uint64_t threshold = histogram.total / 10000;
    for (auto ritf = histogram.data.crbegin(); ritf != histogram.data.crend(); ++ritf)
    {
        info.clippedCount += ritf->second;
        if (ritf->second > threshold)
        {
            if (info.clippedCount - ritf->second < threshold / 10) // overexposed
            {
                info.whiteLevel = ++ritf != histogram.data.rend()? ritf->first : 0;
            }
            else
            {
                info.clippedCount = 0;
                while ((ritf != histogram.data.crbegin()) && ritf->second) ++ritf;
                if (!ritf->second) --ritf;
                info.whiteLevel = ritf->first;
            }
            break;
        }
    }
    return info;
}

ImageSelection::ptr ImageAlgo::getLeftMask(const RawImage::ptr& image, const FilterPattern& channel)
{
    if (!image->leftMask) throw ImageException("getLeftMask: image lacks a left mask");

    ImageChannel::ptr layer = image->getChannel(channel);

    imgsize_t factorh = channel.ydelta == 1? 1 : 2;
    imgsize_t factorw = channel.xdelta == 1? 1 : 2;

    ImageSelection::ptr leftMask = layer->select(0, image->topMask / factorh,
                                                    image->leftMask / factorw,
                                                    imgsize_t(layer->height() - image->topMask / factorh));

    imgsize_t ofsmh = channel.ydelta == 1? 4 : 2; // safety borders
    imgsize_t ofsmw = channel.xdelta == 1? 4 : 2;
    if (ofsmh >= leftMask->height / 4) ofsmw = leftMask->height / 4;
    if (ofsmw >= leftMask->width / 4) ofsmw = leftMask->width / 4;

    return leftMask->select(ofsmw, ofsmh, leftMask->width - ofsmw*2, leftMask->height - ofsmh*2);
}

RawImage::ptr ImageAlgo::dprawProcess(const DPRAW& dpraw, DPRAW::Action action, DPRAW::ProcessMode processMode)
{
    if (!dpraw.imgAB->sameSizeAs(dpraw.imgB))
        throw ImageException("dprawProcess: image and subimage size don't match");

    if (!dpraw.imgAB->leftMask || !dpraw.imgB->leftMask)
        throw ImageException("dprawProcess: required images with masked pixels to evaluate the black point");

    auto blackAB_red = ImageMath::analyze(getLeftMask(dpraw.imgAB, FilterPattern::RGGB_R())).mean;
    auto blackAB_gr1 = ImageMath::analyze(getLeftMask(dpraw.imgAB, FilterPattern::RGGB_G1())).mean;
    auto blackAB_gr2 = ImageMath::analyze(getLeftMask(dpraw.imgAB, FilterPattern::RGGB_G2())).mean;
    auto blackAB_blu = ImageMath::analyze(getLeftMask(dpraw.imgAB, FilterPattern::RGGB_B())).mean;

    auto blackB_red = ImageMath::analyze(getLeftMask(dpraw.imgB, FilterPattern::RGGB_R())).mean;
    auto blackB_gr1 = ImageMath::analyze(getLeftMask(dpraw.imgB, FilterPattern::RGGB_G1())).mean;
    auto blackB_gr2 = ImageMath::analyze(getLeftMask(dpraw.imgB, FilterPattern::RGGB_G2())).mean;
    auto blackB_blu = ImageMath::analyze(getLeftMask(dpraw.imgB, FilterPattern::RGGB_B())).mean;

    auto newImage = RawImage::create(dpraw.imgAB);
    ChannelIterators inAB(dpraw.imgAB);
    ChannelIterators inB(dpraw.imgB);
    ChannelIterators out(newImage);

    auto white = dpraw.white;

    if (action == DPRAW::Action::GetA) // compute the A subframe subtracting B from AB
    {
        int black = int(0.5 + (blackAB_red + blackAB_gr1 + blackAB_gr2 + blackAB_blu) / 4); // similar target

        if (processMode == DPRAW::ProcessMode::Plain)
        {
            while (out)
            {
                out.red = inAB.red >= white? inB.red : 0.5 + (inAB.red - blackAB_red) - (inB.red - blackB_red) + black;
                out.gr1 = inAB.gr1 >= white? inB.gr1 : 0.5 + (inAB.gr1 - blackAB_gr1) - (inB.gr1 - blackB_gr1) + black;
                out.gr2 = inAB.gr2 >= white? inB.gr2 : 0.5 + (inAB.gr2 - blackAB_gr2) - (inB.gr2 - blackB_gr2) + black;
                out.blu = inAB.blu >= white? inB.blu : 0.5 + (inAB.blu - blackAB_blu) - (inB.blu - blackB_blu) + black;
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
                    out.red = 0.5 + (inAB.red++ - blackAB_red) - (inB.red++ - blackB_red) + black;
                    out.gr1 = 0.5 + (inAB.gr1++ - blackAB_gr1) - (inB.gr1++ - blackB_gr1) + black;
                    out.gr2 = 0.5 + (inAB.gr2++ - blackAB_gr2) - (inB.gr2++ - blackB_gr2) + black;
                    out.blu = 0.5 + (inAB.blu++ - blackAB_blu) - (inB.blu++ - blackB_blu) + black;
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
                out.red = inAB.red >= white? inB.red : 0.5 + (inAB.red - blackAB_red) * scale + blackB_red;
                out.gr1 = inAB.gr1 >= white? inB.gr1 : 0.5 + (inAB.gr1 - blackAB_gr1) * scale + blackB_gr1;
                out.gr2 = inAB.gr2 >= white? inB.gr2 : 0.5 + (inAB.gr2 - blackAB_gr2) * scale + blackB_gr2;
                out.blu = inAB.blu >= white? inB.blu : 0.5 + (inAB.blu - blackAB_blu) * scale + blackB_blu;
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
                    out.red = 0.5 + (inAB.red++ - blackAB_red) * scale + blackB_red;
                    out.gr1 = 0.5 + (inAB.gr1++ - blackAB_gr1) * scale + blackB_gr1;
                    out.gr2 = 0.5 + (inAB.gr2++ - blackAB_gr2) * scale + blackB_gr2;
                    out.blu = 0.5 + (inAB.blu++ - blackAB_blu) * scale + blackB_blu;
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
