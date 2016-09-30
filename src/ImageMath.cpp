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

ImageMath::Histogram::ptr ImageMath::buildHistogram(const ImageSelection::ptr& bitmap)
{
    auto info = std::make_shared<ImageMath::Histogram>();
    ImageSelection::Iterator pixel(bitmap);
    while (pixel) ++info->data[pixel++];
    info->total = 0;
    info->mode = 0;
    uint64_t modeFreq = 0;
    for (auto i = info->data.cbegin(); i != info->data.cend(); ++i)
    {
        info->total += i->second;
        if (i->second >= modeFreq)
        {
            modeFreq = i->second;
            info->mode = i->first;
        }
    }
    return info;
}

ImageMath::Stats1 ImageMath::analyze(const ImageSelection::ptr& bitmap)
{
    Stats1 result;
    ImageSelection::Iterator dn(bitmap);
    result.max = result.min = dn;
    long double sum_x = dn;
    long double sum_x2 = dn * dn; // room for gigapixels
    while (++dn) // single-pass algorithm
    {
        if (dn > result.max) result.max = dn; else if (dn < result.min) result.min = dn;
        sum_x += dn;
        sum_x2 += dn * dn;
    }
    long double expectedValue = sum_x / bitmap->pixelCount();
    long double variance = sum_x2 / bitmap->pixelCount() - expectedValue * expectedValue;
    result.mean = double(expectedValue);
    result.stdev = double(std::sqrt(variance));
    return result;
}

ImageMath::Stats2 ImageMath::subtract(const ImageSelection::ptr& bitmapA, const ImageSelection::ptr& bitmapB)
{
    if (!bitmapA->sameAs(bitmapB)) throw ImageException("can't subtract bitmaps of different size/placement");
    Stats2 result;
    ImageSelection::Iterator dnA(bitmapA);
    ImageSelection::Iterator dnB(bitmapB);
    result.a.max = result.a.min = dnA;
    result.b.max = result.b.min = dnB;
    long double sum_A = dnA;
    long double sum_A2 = dnA * dnA;
    long double sum_B = dnB;
    long double sum_B2 = dnB * dnB;
    double delta = dnA - dnB;
    long double sum_d = delta;
    long double sum_d2 = delta * delta;
    while (++dnA && ++dnB)
    {
        if (dnA > result.a.max) result.a.max = dnA; else if (dnA < result.a.min) result.a.min = dnA;
        sum_A += dnA;
        sum_A2 += dnA * dnA;
        if (dnB > result.b.max) result.b.max = dnB; else if (dnB < result.b.min) result.b.min = dnB;
        sum_B += dnB;
        sum_B2 += dnB * dnB;
        delta = dnA - dnB;
        sum_d += delta;
        sum_d2 += delta * delta;
    }
    auto pixels = bitmapA->pixelCount();
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
