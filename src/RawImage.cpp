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

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <sstream>
#include <limits>
#include <arpa/inet.h>
#include "Util.hpp"
#include "RawImage.h"

template <typename T> T toLocalEndian(T number);
template <> uint16_t toLocalEndian(uint16_t number) { return ntohs(number); }
template <> uint32_t toLocalEndian(uint32_t number) { return ntohl(number); }

RawImage::ptr loadPGM(const std::string& fileName, int fd, std::istringstream& header,
                      imgsize_t leftMask, imgsize_t topMask)
{
    uint64_t ww, hh;
    header >> ww;
    header >> hh;
    if ((ww > std::numeric_limits<imgsize_t>::max()) || (hh > std::numeric_limits<imgsize_t>::max()))
    {
        close(fd);
        throw ImageException(VA_STR(fileName << " unsupported file size (" << ww << "x" << hh << ")"));
    }
    imgsize_t width = imgsize_t(ww);
    imgsize_t height = imgsize_t(hh);
    uint64_t maxcolor;
    header >> maxcolor;
    if ((maxcolor < 256) || (maxcolor > 65535))
    {
        close(fd);
        throw ImageException(VA_STR(fileName << " not a 16-bit PGM file"));
    }

    auto image = RawImage::create(width, height, leftMask, topMask);

    uint8_t delim;
    header.read((char *) &delim, 1);

    lseek(fd, header.tellg(), SEEK_SET);

    auto bytes = read(fd, (char *) image->data, image->length);
    close(fd);
    if (bytes < decltype(bytes)(image->length)) throw ImageException(VA_STR("error reading " << fileName));

    bitdepth_t* pixel = image->data;
    for (imgsize_t px = 0; px < image->length; px++)
    {
        *pixel = toLocalEndian(*pixel);
        pixel++;
    }

    return image;
}

RawImage::ptr RawImage::load(const std::string& fileName, imgsize_t leftMask, imgsize_t topMask) // from any supported file
{
    int fd = open(fileName.c_str(), O_RDONLY);
    if (fd < 0) throw ImageException(VA_STR("opening " << fileName << ": " << strerror(errno)));

    char buffer[64];
    auto bytes = read(fd, buffer, sizeof(buffer));
    if (bytes < decltype(bytes)(sizeof(buffer)))
    {
        close(fd);
        throw ImageException(VA_STR(fileName << ": too short file"));
    }

    buffer[sizeof(buffer)-1] = 0;
    std::istringstream header(buffer);
    std::string magic;
    header >> magic;
    if (magic != "P5")
    {
        close(fd);
        throw ImageException(VA_STR(fileName << " seems not to be a valid PGM file"));
    }

    return loadPGM(fileName, fd, header, leftMask, topMask);
}

void RawImage::save(const std::string& fileName) const
{
    std::string format = String::tolower(String::right(fileName, 4));
    if (format != ".dat") throw ImageException(VA_STR("unsupported write file format '" << format << "'"));
    int fd = open(fileName.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0664);
    if (fd < 0) throw ImageException(VA_STR("opening " << fileName << ": " << strerror(errno)));
    auto bytes = write(fd, data, length);
    bool success = bytes == decltype(bytes)(length);
    if (close(fd)) success = false;
    if (!success) throw ImageException(VA_STR("writing " << fileName << ": " << strerror(errno)));
}
