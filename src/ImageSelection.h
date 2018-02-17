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

#ifndef IMAGESELECTION_H_
#define IMAGESELECTION_H_

#include <cstdint>
#include <stdexcept>
#include <string>
#include <memory>

typedef uint16_t bitdepth_t; // enough for 16-bit ADC
typedef uint32_t imgsize_t;

class ImageException : public std::logic_error
{
    public:
        explicit ImageException(const std::string& arg) : std::logic_error(arg) {}
};

struct ImageCrop
{
    const imgsize_t x, y;
    const imgsize_t width, height;
};

class ImageSelection : public ImageCrop // virtualizes a image area selection within a channel
{
        ImageSelection& operator=(const ImageSelection&) = delete;
        ImageSelection(const ImageSelection&) = delete;

    public:

        typedef std::shared_ptr<ImageSelection> ptr;

        explicit ImageSelection(const std::shared_ptr<const class ImageChannel>& imageChannel, const ImageCrop& crop);

        explicit ImageSelection(const std::shared_ptr<const class ImageChannel>& imageChannel,
                                imgsize_t cx, imgsize_t cy,
                                imgsize_t imageWidth, imgsize_t imageHeight)
            : ImageSelection(imageChannel, ImageCrop { cx, cy, imageWidth, imageHeight }) {}

        const std::shared_ptr<const class ImageChannel> channel;

        ImageSelection::ptr select(imgsize_t cx, imgsize_t cy, imgsize_t subSelWidth, imgsize_t subSelHeight) const;

        bitdepth_t& pixel(imgsize_t cx, imgsize_t cy) const; // for random access (5-10 times slower upon compilers)

        bool sameAs(const ImageSelection::ptr& that) const
        {
            return (width == that->width) && (height == that->height) && (x == that->x) && (y == that->y);
        }

        imgsize_t pixelCount() const
        {
            return width * height;
        }

        /* High-performance sequential in-situ fetching of all pixels from left to right and top to bottom
         *
         *     ImageSelection::Iterator pixel(bitmap);
         *
         *     double sum = pixel; // as number gets the current pixel value
         *     while (++pixel)     // pre-increment: goes to the next pixel and returns false if not exists
         *         sum += pixel;   // as number gets the current pixel value
         *
         *     pixel.rewind(); // another pass
         *     while (pixel) // as boolean gets the current pixel existence (not its value)
         *     {
         *         pixel = pixel * pixel; // modify the current pixel (as number gets the current pixel value)
         *         printSquared(pixel++); // post-increment: gets the current pixel value and goes to the next pixel
         *     }
         */
        class Iterator
        {
            public:

                explicit Iterator(const std::shared_ptr<ImageSelection>& imageSelection);
                explicit Iterator(const std::shared_ptr<ImageChannel>& imageChannel);

                inline explicit operator bool() const // if tested as boolean becomes false at end of data
                {
                    return nextRow > 0;
                }

                inline operator bitdepth_t() const // automatic conversion to return the current pixel value
                {
                    if (!nextRow) throw ImageException("No pixel!");
                    bitdepth_t pixelValue = *rawData;
                    return pixelValue;
                }

                inline bitdepth_t operator++(int) // gets the current pixel value plus advances to the next pixel
                {
                    bitdepth_t pixelValue = operator bitdepth_t();
                    next();
                    return pixelValue;
                }

                inline bool operator++() // advances to the next pixel and returns false if there aren't more pixels
                {
                    if (operator bool()) next();
                    return operator bool();
                }

                template <typename Number> inline bitdepth_t operator=(Number pixelValue) const // sets current pixel value
                {
                    if (!nextRow) throw ImageException("No pixel!");
                    bitdepth_t integerValue = (bitdepth_t) pixelValue; // no need to cast on client side
                    *rawData = integerValue;
                    return integerValue;
                }

                inline bitdepth_t operator=(const Iterator& that) const // sets current pixel value
                {
                    return operator=(that.operator bitdepth_t());
                }

                inline void next() // fast position update accounting for the Bayer geometry
                {
                    if (--nextColumn) rawData += xskip;
                    else
                    {
                        nextColumn = selection->width; // this code executed a single time per row of pixels
                        rawData += yskipNext;
                        std::swap(yskipNext, yskipPrev);
                        nextRow--;
                    }
                }

                void rewind() // allow another full iteration of the image selection
                {
                    rawData = rawStartOffset;
                    yskipNext = yskip + yskipShift; // these deal with the pixel column position in the Bayer
                    yskipPrev = yskip - yskipShift; // matrix potentially differing in odd and even rows
                    nextColumn = selection->width;
                    nextRow = selection->height;
                }

                inline imgsize_t column() const { return selection->width - nextColumn; } // current coordinates
                inline imgsize_t row() const { return selection->height - nextRow; }

                const std::shared_ptr<class ImageSelection> selection;

            private:

                bitdepth_t* rawStartOffset;
                imgsize_t xskip;
                imgsize_t yskip;
                imgsize_t yskipShift;

                bitdepth_t* rawData;
                imgsize_t yskipNext;
                imgsize_t yskipPrev;
                imgsize_t nextColumn;
                imgsize_t nextRow;
        };
};

#endif /* IMAGESELECTION_H_ */
