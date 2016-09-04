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

#include "Util.hpp"
#include "RawImage.h"
#include "ImageChannel.h"
#include "ImageSelection.h"

ImageSelection::ImageSelection(
    const std::shared_ptr<const class ImageChannel>& imageChannel,
    imgsize_t cx, imgsize_t cy,
    imgsize_t imageWidth, imgsize_t imageHeight
) : channel(imageChannel), x(cx), y(cy), width(imageWidth), height(imageHeight)
{
    if ((width < 1) || (height < 1)) throw ImageException // save later validations in math operations
    (
        VA_STR("out of range: width(" << width << ") height(" << height << ")")
    );

    if ((uint64_t) x + width > channel->width()) throw ImageException
    (
        VA_STR("out of range: X(" << x << ") + width(" << width << ") beyond " << channel->width())
    );

    if ((uint64_t) y + height > channel->height()) throw ImageException
    (
        VA_STR("out of range: Y(" << y << ") + height(" << height << ") beyond " << channel->height())
    );
}

bitdepth_t ImageSelection::pixel(imgsize_t cx, imgsize_t cy) const
{
    if (cx >= width) throw ImageException(VA_STR("out of range: X(" << cx << ") beyond " << width - 1));
    if (cy >= height) throw ImageException(VA_STR("out of range: Y(" << cy << ") beyond " << height - 1));
    const FilterPattern& bayer = channel->pattern;
    auto offset = ((y + cy) * bayer.ydelta + channel->raw->yalign + bayer.yshift) * channel->raw->width +
                   (x + cx) * bayer.xdelta + channel->raw->xalign + ((y + cy) & 1? bayer.xshift_o : bayer.xshift_e);
    return *(channel->raw->data + offset);
}

ImageSelection::Iterator::Iterator(const std::shared_ptr<const ImageSelection>& imageSelection) : selection(imageSelection)
{
    const ImageChannel::ptr& image = selection->channel;
    const FilterPattern& bayer = image->pattern;

    auto xshift = selection->y & 1? bayer.xshift_o : bayer.xshift_e;
    yskipShift = (selection->y & 1? bayer.xshift_e : bayer.xshift_o) - xshift;

    rawStartOffset = image->raw->data
                   + (selection->y * bayer.ydelta + image->raw->yalign + bayer.yshift) * image->raw->width
                   +  selection->x * bayer.xdelta + image->raw->xalign + xshift;

    yskip = imgsize_t(image->raw->width * bayer.ydelta - (selection->width - 1) * bayer.xdelta);
    xskip = bayer.xdelta;

    rewind();
}
