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
        explicit RawImage(): data(0) {}
        RawImage& operator=(const RawImage&) = delete;
        RawImage(const RawImage&) = delete;

    public:

        typedef std::shared_ptr<const RawImage> ptr; // nobody must change a RAW :D

        virtual ~RawImage() { if(data) delete []data; }

        static RawImage::ptr fromPGM(const std::string& fileName,
                                     imgsize_t xalign = 0, imgsize_t yalign = 0) // create this object from a PGM file
        {
            std::shared_ptr<RawImage> image(new RawImage());
            image->readPGM(fileName);
            image->xalign = xalign < image->width? xalign : 0;
            image->yalign = yalign < image->height? yalign : 0;
            return image;
        }

        ImageChannel::ptr getChannel(const FilterPattern& filterPattern) const
        {
            return ImageChannel::ptr(new ImageChannel(shared_from_this(), filterPattern));
        }

        bitdepth_t* data; // std::vector would require a wasteful and unuseful memory initialization
        imgsize_t width;
        imgsize_t height;
        imgsize_t xalign; // pixels to skip from left & top (odd size in optical black area causing Bayer misalignment)
        imgsize_t yalign;

    private:

        void readPGM(const std::string& fileName);
};

#endif /* RAWIMAGE_H_ */
