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
#include "ImageMath.h"

ImageMath::Histogram ImageMath::buildHistogram(const ImageSelection::ptr& bitmap)
{
    ImageMath::Histogram info;
    ImageSelection::Iterator area(bitmap);
    while (area.hasPixel()) ++info.data[area.nextPixel()];
    info.total = 0;
    info.mode = 0;
    uint64_t modeFreq = 0;
    for (auto i = info.data.cbegin(); i != info.data.cend(); ++i)
    {
        info.total += i->second;
        if (i->second >= modeFreq)
        {
            modeFreq = i->second;
            info.mode = i->first;
        }
    }
    return info;
}

ImageMath::Highlights ImageMath::getHighlights(const Histogram& histogram)
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

ImageMath::Stats1 ImageMath::analyze(const ImageSelection::ptr& bitmap)
{
    Stats1 result;
    ImageSelection::Iterator area(bitmap);
    bitdepth_t dn = area.nextPixel();
    result.max = result.min = dn;
    long double sum_x = dn;
    long double sum_x2 = dn * dn; // room for gigapixels
    while (area.hasPixel()) // single-pass algorithm
    {
        dn = area.nextPixel();
        if (dn > result.max) result.max = dn; else if (dn < result.min) result.min = dn;
        sum_x += dn;
        sum_x2 += dn * dn;
    }
    auto pixels = bitmap->width * bitmap->height;
    long double expectedValue = sum_x / pixels;
    long double variance = sum_x2 / pixels - expectedValue * expectedValue;
    result.mean = double(expectedValue);
    result.stdev = double(std::sqrt(variance));
    return result;
}

ImageMath::Stats2 ImageMath::subtract(const ImageSelection::ptr& bitmapA, const ImageSelection::ptr& bitmapB)
{
    if (!bitmapA->sameAs(bitmapB)) throw ImageException("can't subtract bitmaps of different size/placement");
    Stats2 result;
    ImageSelection::Iterator areaA(bitmapA);
    ImageSelection::Iterator areaB(bitmapB);
    bitdepth_t dnA = areaA.nextPixel();
    bitdepth_t dnB = areaB.nextPixel();
    result.a.max = result.a.min = dnA;
    result.b.max = result.b.min = dnB;
    long double sum_A = dnA;
    long double sum_A2 = dnA * dnA;
    long double sum_B = dnB;
    long double sum_B2 = dnB * dnB;
    double delta = dnA - dnB;
    long double sum_d = delta;
    long double sum_d2 = delta * delta;
    while (areaA.hasPixel())
    {
        dnA = areaA.nextPixel();
        if (dnA > result.a.max) result.a.max = dnA; else if (dnA < result.a.min) result.a.min = dnA;
        sum_A += dnA;
        sum_A2 += dnA * dnA;
        dnB = areaB.nextPixel();
        if (dnB > result.b.max) result.b.max = dnB; else if (dnB < result.b.min) result.b.min = dnB;
        sum_B += dnB;
        sum_B2 += dnB * dnB;
        delta = dnA - dnB;
        sum_d += delta;
        sum_d2 += delta * delta;
    }
    auto pixels = bitmapA->width * bitmapA->height;
    long double expectedValue = sum_d / pixels;
    long double variance = sum_d2 / pixels - expectedValue * expectedValue;
    result.stdev = double(std::sqrt(variance / 2));
    expectedValue = sum_A / pixels;
    variance = sum_A2 / pixels - expectedValue * expectedValue;
    result.a.mean = double(expectedValue);
    result.a.stdev = double(std::sqrt(variance));
    expectedValue = sum_B / pixels;
    variance = sum_B2 / pixels - expectedValue * expectedValue;
    result.b.mean = double(expectedValue);
    result.b.stdev = double(std::sqrt(variance));
    return result;
}
