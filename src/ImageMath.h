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

#ifndef IMAGEMATH_H_
#define IMAGEMATH_H_

#include <map>
#include "ImageSelection.h"

class ImageMath
{
    public:

        struct Stats1
        {
            bitdepth_t min;
            bitdepth_t max;
            double mean;
            double stdev;
        };

        struct Stats2
        {
            Stats1 a;
            Stats1 b;
            double stdev; // of the subtraction
        };

        struct Histogram
        {
            typedef std::shared_ptr<Histogram> ptr;
            typedef std::map<bitdepth_t, imgsize_t> Frequencies;
            Frequencies data;
            uint64_t total;
            bitdepth_t mode; // statistical mode
        };

        static Histogram::ptr buildHistogram(const ImageSelection::ptr& bitmap);
        static Stats1 analyze(const ImageSelection::ptr& bitmap);
        static Stats2 subtract(const ImageSelection::ptr& bitmapA, const ImageSelection::ptr& bitmapB);
};

#endif /* IMAGEMATH_H_ */
