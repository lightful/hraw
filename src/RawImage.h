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

#ifndef RAWIMAGE_H_
#define RAWIMAGE_H_

#include <map>
#include "ImageChannel.h"

/* RawImage holds the RAW data on memory, safely sharing it with another objects.
 * This and every other object in the model will be automatically deleted. The memory
 * allocated by a image will be released when the last object coming from it goes out
 * of scope, regardless its position in the hierarchy or the destruction order.
 */
class RawImage : public std::enable_shared_from_this<RawImage>
{
    public:

        typedef std::shared_ptr<RawImage> ptr;

        struct Masked // masked pixels (optical black area)
        {
            typedef std::shared_ptr<Masked> ptr;
            imgsize_t left;
            imgsize_t top;
        };

        typedef std::map<ImageFilter::Code, double> BlackLevel;

    private:

        explicit RawImage(imgsize_t width, imgsize_t height, const Masked& opticalBlack)
          : length(imgsize_t(sizeof(bitdepth_t) * width * height)),
            data(new bitdepth_t[length]),
            rowPixels(width), colPixels(height),
            masked { opticalBlack.left < width? opticalBlack.left : 0, opticalBlack.top < height? opticalBlack.top : 0 }
        {}

        RawImage& operator=(const RawImage&) = delete;
        RawImage(const RawImage&) = delete;

    public:

        virtual ~RawImage() { if(data) delete []data; }

        static RawImage::ptr create(imgsize_t width, imgsize_t height, const RawImage::Masked& opticalBlack)
        {
            return RawImage::ptr(new RawImage(width, height, opticalBlack));
        }

        static RawImage::ptr layout(const RawImage::ptr& config) // memory allocated but data not copied
        {
            return create(config->rowPixels, config->colPixels, config->masked);
        }

        static RawImage::ptr load(const std::string& fileName, const RawImage::Masked::ptr& opticalBlack = Masked::ptr());

        void save(const std::string& fileName) const;

        ImageChannel::ptr getChannel(const ImageFilter& imageFilter) const
        {
            return ImageChannel::ptr(new ImageChannel(shared_from_this(), imageFilter));
        }

        bool sameSizeAs(const RawImage::ptr& that) const
        {
            return (rowPixels == that->rowPixels) && (colPixels == that->colPixels)
                && (masked.left == that->masked.left) && (masked.top == that->masked.top);
        }

        imgsize_t pixelCount(bool effective = true) const
        {
            return (rowPixels - (effective? masked.left : 0)) * (colPixels - (effective? masked.top : 0));
        }

        inline imgsize_t bayerStart() const { return yalign() * rowPixels + xalign(); }

        inline imgsize_t bayerWidth() const { return rowPixels - xalign(); }

        inline imgsize_t bayerHeight() const { return colPixels - yalign(); }

        const imgsize_t length;
        bitdepth_t* const data; // std::vector would require a wasteful and unuseful memory initialization

        const imgsize_t rowPixels; // physical image dimensions
        const imgsize_t colPixels;

        const Masked masked;

        BlackLevel blackLevel;
        std::shared_ptr<bitdepth_t> whiteLevel;

        std::string name;

        bool hasBlackLevel() const { return !blackLevel.empty(); }

    private:

        inline imgsize_t xalign() const { return masked.left & 1; } // pixels to skip from left & top (odd size in
        inline imgsize_t yalign() const { return masked.top & 1; }  // optical black area causing Bayer misalignment)
};

#endif /* RAWIMAGE_H_ */
