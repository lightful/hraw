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
#include "RawImage.h"
#include "ImageChannel.h"

imgsize_t ImageChannel::width() const
{
    return (raw->width - raw->xalign) / pattern.xdelta;
}

imgsize_t ImageChannel::height() const
{
    return (raw->height - raw->yalign) / pattern.ydelta;
}

ImageSelection::ptr ImageChannel::select(double partsPerUnit) const
{
    uint64_t area = uint64_t(partsPerUnit * width() * height());
    imgsize_t side = imgsize_t(std::sqrt(area));
    imgsize_t x = imgsize_t((width()-side)/2);
    imgsize_t y = imgsize_t((height()-side)/2);
    return ImageSelection::ptr(new ImageSelection(shared_from_this(), x, y, side, side));
}
