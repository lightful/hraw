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

#ifndef IMAGECHANNEL_H_
#define IMAGECHANNEL_H_

#include <istream>
#include <ostream>
#include "ImageSelection.h"

struct ImageFilter // allows to represent any (simple) periodic pixel pattern
{
    enum class Code { R, G1, G2, B, G, RGB };

    const Code code;
    const imgsize_t xshift_e; // 0  1  0  1 (even rows)
    const imgsize_t xshift_o; // 0  1  0  1 (odd rows)
    const imgsize_t yshift;   // 0  0  1  1
    const imgsize_t xdelta;   // 2  2  2  2  <-- example for a RGGB matrix:
    const imgsize_t ydelta;   // 2  2  2  2                R  G1
                              // R G1 G2  B                G2  B

    static ImageFilter R()   { return ImageFilter { Code::R,   0, 0, 0, 2, 2 }; }
    static ImageFilter G1()  { return ImageFilter { Code::G1,  1, 1, 0, 2, 2 }; }
    static ImageFilter G2()  { return ImageFilter { Code::G2,  0, 0, 1, 2, 2 }; }
    static ImageFilter G()   { return ImageFilter { Code::G,   1, 0, 0, 2, 1 }; } // both green channels
    static ImageFilter B()   { return ImageFilter { Code::B,   1, 1, 1, 2, 2 }; }
    static ImageFilter RGB() { return ImageFilter { Code::RGB, 0, 0, 0, 1, 1 }; } // full plain image (all pixels)

    static ImageFilter create(Code code)
    {
        switch (code)
        {
            case Code::R:   return R();
            case Code::G1:  return G1();
            case Code::G2:  return G2();
            case Code::G:   return G();
            case Code::B:   return B();
            case Code::RGB: return RGB();
        }
        throw;
    }
};

class ImageChannel : public std::enable_shared_from_this<ImageChannel> // virtualizes a color channel selection
{
        ImageChannel& operator=(const ImageChannel&) = delete;
        ImageChannel(const ImageChannel&) = delete;

    public:

        typedef std::shared_ptr<ImageChannel> ptr;

        explicit ImageChannel(const std::shared_ptr<const class RawImage>& rawImage, const ImageFilter& imageFilter)
          : raw(rawImage),
            filter(imageFilter)
        {}

        const std::shared_ptr<const class RawImage> raw;
        const ImageFilter filter;

        imgsize_t width() const;
        imgsize_t height() const;

        double blackLevel() const;

        ImageSelection::ptr select(bool unmasked = false) const; // full channel

        ImageSelection::ptr select(imgsize_t cx, imgsize_t cy, imgsize_t selectedWidth, imgsize_t selectedHeight) const
        {
            return ImageSelection::ptr(new ImageSelection(shared_from_this(), cx, cy, selectedWidth, selectedHeight));
        }

        ImageSelection::ptr select(const std::shared_ptr<ImageCrop>& crop) const
        {
            return crop? ImageSelection::ptr(new ImageSelection(shared_from_this(), *crop)) : select();
        }

        ImageSelection::ptr getLeftMask(bool safetyCrop = true, bool overlappingTop = false) const;
};

std::istream& operator>>(std::istream& in, ImageFilter::Code& fc);
std::ostream& operator<<(std::ostream& out, const ImageFilter::Code& fc);

#endif /* IMAGECHANNEL_H_ */
