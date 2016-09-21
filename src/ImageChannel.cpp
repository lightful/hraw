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
    return raw->bayerWidth() / pattern.xdelta;
}

imgsize_t ImageChannel::height() const
{
    return raw->bayerHeight() / pattern.ydelta;
}

std::ostream& operator<<(std::ostream& out, const FilterPattern& fp)
{
    if      (fp == FilterPattern::RGGB_R())  return out << "R";
    else if (fp == FilterPattern::RGGB_G1()) return out << "G1";
    else if (fp == FilterPattern::RGGB_G2()) return out << "G2";
    else if (fp == FilterPattern::RGGB_G())  return out << "G";
    else if (fp == FilterPattern::RGGB_B())  return out << "B";
    else if (fp == FilterPattern::FULL())    return out << "RGB";
    else                                     return out << "?";
}

std::istream& operator>>(std::istream& in, FilterPattern& fp)
{
    std::string str;
    if (in >> str)
    {
        str = String::toupper(str);
             if (!str.compare("R"))   fp = FilterPattern::RGGB_R();
        else if (!str.compare("G1"))  fp = FilterPattern::RGGB_G1();
        else if (!str.compare("G2"))  fp = FilterPattern::RGGB_G2();
        else if (!str.compare("G"))   fp = FilterPattern::RGGB_G();
        else if (!str.compare("B"))   fp = FilterPattern::RGGB_B();
        else if (!str.compare("RGB")) fp = FilterPattern::FULL();
        else in.setstate(std::ios_base::failbit);
    }
    return in;
}
