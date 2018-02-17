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

#include <cstring>
#include <cerrno>
#include <sstream>
#include <fstream>
#include <limits>
#include "Util.hpp"
#include "RawImage.h"

inline bool isBigEndian()
{
    static union { uint16_t i; uint8_t c; } endianness { 0x0102 };
    return endianness.c == 0x01;
}

template <typename T> T endian(T n); // Winsock2.h would define conflicting macros like 'RGB' or (a lowercase!) 'max'...

template <> uint16_t endian(uint16_t n)
{
    return isBigEndian()? n : uint16_t(n >> 8 | n << 8);
}

template <> uint32_t endian(uint32_t n)
{
    return isBigEndian()? n : uint32_t(n >> 24 | (n & 0xFF0000) >> 8 | (n & 0xFF00) << 8 | n << 24);
}

std::string getLastError() // no C++11 portable error reporting support actually beyond failbit
{
    auto ecode = errno;
    char msg[1024];
#ifdef _WIN32
    strerror_s(msg, sizeof(msg)-1, ecode);
    return msg;
#else
    return strerror_r(ecode, msg, sizeof(msg)-1);
#endif
}

RawImage::ptr loadPGM(const std::string& fileName,
                      std::ifstream& in, std::istringstream& header,
                      const RawImage::Masked& opticalBlack)
{
    uint64_t ww, hh;
    header >> ww;
    header >> hh;
    if ((ww > std::numeric_limits<imgsize_t>::max()) || (hh > std::numeric_limits<imgsize_t>::max()))
        throw ImageException(VA_STR(fileName << " unsupported file size (" << ww << "x" << hh << ")"));

    imgsize_t width = imgsize_t(ww);
    imgsize_t height = imgsize_t(hh);
    uint64_t maxcolor;
    header >> maxcolor;
    if ((maxcolor < 256) || (maxcolor > 65535))
        throw ImageException(VA_STR(fileName << " not a 16-bit PGM file"));

    auto image = RawImage::create(width, height, opticalBlack);

    auto pd = fileName.find_last_of("\\/");
    image->name = fileName.substr((pd == std::string::npos)? 0 : pd + 1);

    uint8_t delim;
    header.read((char *) &delim, 1);

    in.seekg(header.tellg(), std::ios::beg);

    if (!in.read((char *) image->data, image->length))
        throw ImageException(VA_STR("error reading " << fileName));

    bitdepth_t* pixel = image->data;
    for (imgsize_t px = 0; px < image->length; px++)
    {
        *pixel = endian(*pixel);
        pixel++;
    }

    return image;
}

RawImage::ptr RawImage::load(const std::string& fileName, const Masked::ptr& opticalBlack) // from any supported file
{
    std::ifstream in(fileName.c_str(), std::ios::binary);
    if (!in) throw ImageException(VA_STR("opening " << fileName << ": " << getLastError()));

    char buffer[64];
    if (!in.read(buffer, sizeof(buffer)))
        throw ImageException(VA_STR(fileName << ": too short file"));

    buffer[sizeof(buffer)-1] = 0;
    std::istringstream header(buffer);
    std::string magic;
    header >> magic;
    if (magic != "P5")
        throw ImageException(VA_STR(fileName << " seems not to be a valid PGM file"));

    return loadPGM(fileName, in, header, opticalBlack? *opticalBlack : RawImage::Masked { 0, 0 });
}

void RawImage::save(const std::string& fileName) const
{
    std::string format = String::tolower(String::right(fileName, 4));
    bool isDat = format == ".dat";
    bool isPGM = format == ".pgm";
    if (!isDat && !isPGM) throw ImageException(VA_STR("unsupported write file format '" << format << "'"));
    std::ofstream out(fileName.c_str(), std::ios::binary);
    if (!out) throw ImageException(VA_STR("error opening " << fileName << ": " << getLastError()));
    try
    {
        std::shared_ptr<const RawImage> toSave = shared_from_this();
        if (isPGM)
        {
            std::string header = VA_STR("P5\n" << rowPixels << " " << colPixels << "\n65535\n");
            if (!out.write(header.c_str(), std::streamsize(header.length()))) throw true;
            try { toSave = RawImage::create(rowPixels, colPixels, masked); } catch(...) { throw true; }
            std::memcpy(toSave->data, data, length);
            bitdepth_t* pixel = toSave->data;
            for (imgsize_t px = 0; px < toSave->length; px++)
            {
                *pixel = endian(*pixel);
                pixel++;
            }
        }
        if (!out.write((const char*) toSave->data, toSave->length)) throw true;
        out.close();
        if (out.fail()) throw true;
    }
    catch (bool&)
    {
        std::string reason = VA_STR("error writing " << fileName << ": " << getLastError());
        throw ImageException(reason);
    }
}
