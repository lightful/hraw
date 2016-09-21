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

#ifndef RAWIMAGE_H_
#define RAWIMAGE_H_

#include "ImageChannel.h"

/* RawImage holds the RAW data on memory, safely sharing it with another objects.
 * This and every other object in the model will be automatically deleted. The memory
 * allocated by a image will be released when the last object coming from it goes out
 * of scope, regardless its position in the hierarchy or the destruction order.
 */
class RawImage : public std::enable_shared_from_this<RawImage>
{
        explicit RawImage(imgsize_t width, imgsize_t height, imgsize_t widthMasked, imgsize_t heightMasked)
          : length(imgsize_t(sizeof(bitdepth_t) * width * height)),
            data(new bitdepth_t[length]),
            rowPixels(width), colPixels(height),
            leftMask(widthMasked < width? widthMasked : 0), topMask(heightMasked < height? heightMasked : 0)
        {}

        RawImage& operator=(const RawImage&) = delete;
        RawImage(const RawImage&) = delete;

    public:

        typedef std::shared_ptr<RawImage> ptr;

        virtual ~RawImage() { if(data) delete []data; }

        static RawImage::ptr create(imgsize_t width, imgsize_t height, imgsize_t leftMask, imgsize_t topMask)
        {
            return RawImage::ptr(new RawImage(width, height, leftMask, topMask));
        }

        static RawImage::ptr create(const RawImage::ptr& config) // data allocated but not copied
        {
            return create(config->rowPixels, config->colPixels, config->leftMask, config->topMask);
        }

        static RawImage::ptr load(const std::string& fileName, imgsize_t leftMask = 0, imgsize_t topMask = 0);

        void save(const std::string& fileName) const;

        ImageChannel::ptr getChannel(const FilterPattern& filterPattern)
        {
            return ImageChannel::ptr(new ImageChannel(shared_from_this(), filterPattern));
        }

        bool sameSizeAs(const RawImage::ptr& that) const
        {
            return (rowPixels == that->rowPixels) && (colPixels == that->colPixels)
                && (leftMask == that->leftMask) && (topMask == that->topMask);
        }

        imgsize_t pixelCount(bool effective = true) const
        {
            return (rowPixels - (effective? leftMask : 0)) * (colPixels - (effective? topMask : 0));
        }

        inline imgsize_t bayerStart() const { return yalign() * rowPixels + xalign(); }

        inline imgsize_t bayerWidth() const { return rowPixels - xalign(); }

        inline imgsize_t bayerHeight() const { return colPixels - yalign(); }

        const imgsize_t length;
        bitdepth_t* const data; // std::vector would require a wasteful and unuseful memory initialization

        const imgsize_t rowPixels; // physical image dimensions
        const imgsize_t colPixels;

        const imgsize_t leftMask; // masked pixels (optical black area)
        const imgsize_t topMask;

    private:

        inline imgsize_t xalign() const { return leftMask & 1; } // pixels to skip from left & top (odd size in
        inline imgsize_t yalign() const { return topMask & 1; }  // optical black area causing Bayer misalignment)
};

#endif /* RAWIMAGE_H_ */
