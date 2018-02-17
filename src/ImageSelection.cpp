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

ImageSelection::ImageSelection(const std::shared_ptr<const ImageChannel>& imageChannel, const ImageCrop& crop)
  : ImageCrop(crop), channel(imageChannel)
{
    if ((width < 1) || (height < 1)) throw ImageException // save later validations in math operations
    (
        VA_STR("out of range: width(" << width << ") height(" << height << ")")
    );

    if ((uint64_t) x + width > channel->width()) throw ImageException
    (
        VA_STR("out of range: X(" << x << ") by width(" << width << ") beyond " << channel->width())
    );

    if ((uint64_t) y + height > channel->height()) throw ImageException
    (
        VA_STR("out of range: Y(" << y << ") by height(" << height << ") beyond " << channel->height())
    );
}

ImageSelection::ptr ImageSelection::select(imgsize_t cx, imgsize_t cy, imgsize_t subSelWidth, imgsize_t subSelHeight) const
{
    if ((uint64_t) cx + subSelWidth > width) throw ImageException
    (
        VA_STR("out of range: X(" << cx << ") by width(" << subSelWidth << ") beyond " << width)
    );

    if ((uint64_t) cy + subSelHeight > height) throw ImageException
    (
        VA_STR("out of range: Y(" << cy << ") by height(" << subSelHeight << ") beyond " << height)
    );

    return ImageSelection::ptr(new ImageSelection(channel, x + cx, y + cy, subSelWidth, subSelHeight));
}

bitdepth_t& ImageSelection::pixel(imgsize_t cx, imgsize_t cy) const
{
    if (cx >= width) throw ImageException(VA_STR("out of range: X(" << cx << ") beyond " << width - 1));
    if (cy >= height) throw ImageException(VA_STR("out of range: Y(" << cy << ") beyond " << height - 1));
    const ImageFilter& bayer = channel->filter;
    auto offset = ((y + cy) * bayer.ydelta + bayer.yshift) * channel->raw->rowPixels +
                   (x + cx) * bayer.xdelta + ((y + cy) & 1? bayer.xshift_o : bayer.xshift_e);
    return channel->raw->data[channel->raw->bayerStart() + offset];
}

ImageSelection::Iterator::Iterator(const std::shared_ptr<ImageSelection>& imageSelection) : selection(imageSelection)
{
    auto& image = selection->channel;
    const ImageFilter& bayer = image->filter;

    auto xshift = selection->y & 1? bayer.xshift_o : bayer.xshift_e;
    yskipShift = (selection->y & 1? bayer.xshift_e : bayer.xshift_o) - xshift;

    rawStartOffset = image->raw->data + image->raw->bayerStart()
                   + (selection->y * bayer.ydelta + bayer.yshift) * image->raw->rowPixels
                   +  selection->x * bayer.xdelta + xshift;

    yskip = imgsize_t(image->raw->rowPixels * bayer.ydelta - (selection->width - 1) * bayer.xdelta);
    xskip = bayer.xdelta;

    rewind();
}

ImageSelection::Iterator::Iterator(const std::shared_ptr<ImageChannel>& imageChannel) : Iterator(imageChannel->select())
{
}
