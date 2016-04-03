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

class ImageSelection // virtualizes a image area selection within a channel
{
        ImageSelection& operator=(const ImageSelection&) = delete;
        ImageSelection(const ImageSelection&) = delete;

    public:

        typedef std::shared_ptr<const ImageSelection> ptr;

        explicit ImageSelection(const std::shared_ptr<const class ImageChannel>& imageChannel,
                                imgsize_t cx, imgsize_t cy,
                                imgsize_t imageWidth, imgsize_t imageHeight);

        const std::shared_ptr<const class ImageChannel> channel;

        const imgsize_t x, y;
        const imgsize_t width, height;

        bitdepth_t pixel(imgsize_t cx, imgsize_t cy) const; // for (slower) random access

        bool sameAs(const ImageSelection::ptr& that) const
        {
            return (width == that->width) && (height == that->height) && (x == that->x) && (y == that->y);
        }

        class Iterator // fast sequential fetching of all pixels from left to right and top to bottom
        {
                Iterator& operator=(const Iterator&) = delete;
                Iterator(const Iterator&) = delete;

            public:

                explicit Iterator(const std::shared_ptr<const ImageSelection>& imageSelection);

                inline bool hasPixel() const { return nextRow; } // becomes false at end of data

                inline bitdepth_t nextPixel() // high performance in-situ sequential fetching
                {
                    if (!nextRow) throw ImageException("nextPixel EOD");
                    bitdepth_t pixelValue = *rawData;
                    if (--nextColumn) rawData += xskip; // jumps accounting for the Bayer geometry
                    else
                    {
                        nextColumn = selection->width;
                        rawData += yskip;
                        nextRow--;
                    }
                    return pixelValue;
                }

                void rewind() // allow another full iteration
                {
                    rawData = rawStartOffset;
                    nextColumn = selection->width;
                    nextRow = selection->height;
                }

                const std::shared_ptr<const class ImageSelection> selection;

            private:

                bitdepth_t* rawStartOffset;
                imgsize_t xskip;
                imgsize_t yskip;

                bitdepth_t* rawData;
                imgsize_t nextColumn;
                imgsize_t nextRow;
        };
};

#endif /* IMAGESELECTION_H_ */
