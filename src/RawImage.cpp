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

void RawImage::readPGM(const std::string& fileName)
{
    if (data) throw ImageException("not a clean instance");

    int fd = open(fileName.c_str(), O_RDONLY);
    if (fd < 0) throw ImageException(VA_STR("opening " << fileName << ": " << strerror(errno)));

    char buffer[64];
    if (read(fd, buffer, sizeof(buffer)) < ssize_t(sizeof(buffer)))
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
    uint64_t ww, hh;
    header >> ww;
    header >> hh;
    if ((ww > std::numeric_limits<imgsize_t>::max()) || (hh > std::numeric_limits<imgsize_t>::max()))
    {
        close(fd);
        throw ImageException(VA_STR(fileName << " unsupported file size (" << ww << "x" << hh << ")"));
    }
    width = imgsize_t(ww);
    height = imgsize_t(hh);
    uint64_t maxcolor;
    header >> maxcolor;
    if ((maxcolor < 256) || (maxcolor > 65535))
    {
        close(fd);
        throw ImageException(VA_STR(fileName << " not a 16-bit PGM file"));
    }
    uint8_t delim;
    header.read((char *) &delim, 1);

    lseek(fd, header.tellg(), SEEK_SET);

    std::size_t imageSize = sizeof(bitdepth_t) * width * height;

    data = new bitdepth_t[imageSize];
    ssize_t bytes = read(fd, (char *) data, imageSize);
    close(fd);
    if (bytes < ssize_t(imageSize))
    {
        delete []data;
        data = 0;
        throw ImageException(VA_STR("error reading " << fileName));
    }

    bitdepth_t* pixel = data;
    for (std::size_t px = 0; px < imageSize; px++)
    {
        *pixel = toLocalEndian(*pixel);
        pixel++;
    }
}
