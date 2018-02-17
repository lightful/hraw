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
    auto ep = fileName.find(".");
    if (ep == std::string::npos) ep = 0;
    std::string format = String::tolower(fileName.substr(ep));
    bool isDat = format == ".dat";
    bool isPGM = format == ".pgm";
    bool isPPM = format == ".ppm";
    bool isTIFF = format == ".tiff";
    if (!isDat && !isPGM && !isPPM && !isTIFF)
        throw ImageException(VA_STR("unsupported write file format '" << format << "'"));
    std::ofstream out(fileName.c_str(), std::ios::binary);
    if (!out) throw ImageException(VA_STR("error opening " << fileName << ": " << getLastError()));
    try
    {
        std::shared_ptr<const RawImage> toSave = shared_from_this();
        if (isPGM || isPPM)
        {
            std::string header = VA_STR("P" << (isPGM? '5' : '6') << "\n"
                                            << (isPGM? rowPixels : rowPixels/3) << " " << colPixels << "\n65535\n");
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
        else if (isTIFF)
        {
            auto write16 = [&out](uint16_t value) { if (!out.write((const char*) &value, sizeof(value))) throw true; };
            auto write32 = [&out](uint32_t value) { if (!out.write((const char*) &value, sizeof(value))) throw true; };
            if (!(out << (isBigEndian()? "MM" : "II"))) throw true;
            write16(42); // TIFF version
            uint32_t ifd_offset = 8;
            uint16_t ifd_entries = 8;
            uint32_t no_next_ifd = 0;
            uint32_t samplesPerPixel = 3;
            uint16_t bitdepth = 16;
            uint32_t bitdepth__offset = uint32_t(ifd_offset + sizeof(ifd_entries) + ifd_entries * 12u + sizeof(no_next_ifd));
            uint32_t image_offset = uint32_t(bitdepth__offset + samplesPerPixel * sizeof(bitdepth));
            write32(ifd_offset);
            write16(ifd_entries);
            write16(0x100); write16(3); write32(1); write32(rowPixels/3);      // ImageWidth
            write16(0x101); write16(3); write32(1); write32(colPixels);        // ImageLength
            write16(0x102); write16(3); write32(3); write32(bitdepth__offset); // BitsPerSample
            write16(0x106); write16(3); write32(1); write32(2);                // PhotometricInterpretation (2: RGB)
            write16(0x111); write16(4); write32(1); write32(image_offset);     // StripOffsets
            write16(0x115); write16(3); write32(1); write32(samplesPerPixel);  // SamplesPerPixel
            write16(0x116); write16(3); write32(1); write32(colPixels);        // RowsPerStrip
            write16(0x117); write16(4); write32(1); write32(toSave->length);   // StripByteCounts
            write32(no_next_ifd);
            write16(bitdepth); write16(bitdepth); write16(bitdepth); // bits per sample (R,G,B)
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
