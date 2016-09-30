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

#include "RawImage.h"
#include "ImageChannel.h"
#include "Util.hpp"

imgsize_t ImageChannel::width() const
{
    return raw->bayerWidth() / filter.xdelta;
}

imgsize_t ImageChannel::height() const
{
    return raw->bayerHeight() / filter.ydelta;
}

double ImageChannel::blackLevel() const
{
    RawImage::BlackLevel::const_iterator fcode = raw->blackLevel.find(filter.code);
    if (fcode != raw->blackLevel.cend()) return fcode->second;
    throw ImageException("blackLevel not defined");
}

ImageSelection::ptr ImageChannel::getLeftMask(bool safetyCrop, bool overlappingTop) const
{
    if (!raw->masked.left) throw ImageException("getLeftMask: image lacks a left mask");

    imgsize_t factorh = filter.ydelta == 1? 1 : 2;
    imgsize_t factorw = filter.xdelta == 1? 1 : 2;

    imgsize_t cy = (overlappingTop? 0 : raw->masked.top) / factorh;
    ImageSelection::ptr leftMask = select(0, cy, raw->masked.left / factorw, height() - cy);

    if (!safetyCrop) return leftMask;

    imgsize_t ofsmh = filter.ydelta == 1? 4 : 2; // safety borders
    imgsize_t ofsmw = filter.xdelta == 1? 4 : 2;
    if (ofsmh >= leftMask->height / 4) ofsmw = leftMask->height / 4;
    if (ofsmw >= leftMask->width / 4) ofsmw = leftMask->width / 4;

    return leftMask->select(ofsmw, ofsmh, leftMask->width - ofsmw*2, leftMask->height - ofsmh*2);
}

std::istream& operator>>(std::istream& in, ImageFilter::Code& fc)
{
    std::string str;
    if (in >> str)
    {
        str = String::toupper(str);
             if (!str.compare("R"))   fc = ImageFilter::Code::R;
        else if (!str.compare("G1"))  fc = ImageFilter::Code::G1;
        else if (!str.compare("G2"))  fc = ImageFilter::Code::G2;
        else if (!str.compare("G"))   fc = ImageFilter::Code::G;
        else if (!str.compare("B"))   fc = ImageFilter::Code::B;
        else if (!str.compare("RGB")) fc = ImageFilter::Code::RGB;
        else
        {
            fc = ImageFilter::Code::RGB;
            in.setstate(std::ios_base::failbit);
        }
    }
    return in;
}

std::ostream& operator<<(std::ostream& out, const ImageFilter::Code& fc)
{
    switch (fc)
    {
        case ImageFilter::Code::R:   return out << "R";
        case ImageFilter::Code::G1:  return out << "G1";
        case ImageFilter::Code::G2:  return out << "G2";
        case ImageFilter::Code::G:   return out << "G";
        case ImageFilter::Code::B:   return out << "B";
        case ImageFilter::Code::RGB: return out << "RGB";
    }
    throw; // silent pointless gcc warning (potential undefined behaviour not originated here)
}
