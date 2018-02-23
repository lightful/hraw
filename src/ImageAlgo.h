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

#ifndef IMAGEALGO_H_
#define IMAGEALGO_H_

#include <vector>
#include <istream>
#include "RawImage.h"
#include "ImageMath.h"

class ImageAlgo
{
    public:

        struct Highlights // less unreliable when got from full-size channel histograms
        {
            bitdepth_t whiteLevel;  // highest non clipped DN
            imgsize_t clippedCount; // amount of pixels
        };

        struct DPRAW // Canon Dual Pixel RAW
        {
            const RawImage::ptr imgAB;
            const RawImage::ptr imgB;
            enum class Action { GetA, Blend };
            enum class ProcessMode { Plain, Bayer };
            bitdepth_t white;
            std::shared_ptr<double> shiftEV; // imgAB EV shift for blending
        };

        static void setBlackLevel(const RawImage::ptr& image, std::vector<double> blackPoints);
        static void setWhiteLevel(const RawImage::ptr& image, std::shared_ptr<bitdepth_t> whitePoint);

        static Highlights getHighlights(const ImageMath::Histogram::ptr& histogram);

        static RawImage::ptr clipping(const RawImage::ptr& input);

        static RawImage::ptr dprawProcess(const DPRAW& dpraw, DPRAW::Action action, DPRAW::ProcessMode processMode);
};

std::istream& operator>>(std::istream& in, ImageAlgo::DPRAW::Action& action);
std::istream& operator>>(std::istream& in, ImageAlgo::DPRAW::ProcessMode& processMode);

#endif /* IMAGEALGO_H_ */
