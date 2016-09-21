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

#ifndef UTIL_HPP_
#define UTIL_HPP_

#include <sstream>
#include <string>
#include <cctype>
#include <algorithm>
#include <cstdint>
#include <chrono>

#define VA_STR(x) dynamic_cast<std::ostringstream const&>(std::ostringstream().flush() << x).str()

struct String
{
    static std::string tolower(const std::string& str)
    {
        std::string lcase = str;
        std::transform(lcase.begin(), lcase.end(), lcase.begin(), ::tolower);
        return lcase;
    }

    static std::string toupper(const std::string& str)
    {
        std::string lcase = str;
        std::transform(lcase.begin(), lcase.end(), lcase.begin(), ::toupper);
        return lcase;
    }

    static std::string right(const std::string& str, std::string::size_type count)
    {
        return str.substr(str.size() - std::min(count, str.size()));
    }
};

struct Util
{
    static int64_t epochNs() // nanoseconds since 1970
    {
        auto nowIs = std::chrono::system_clock::now().time_since_epoch();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(nowIs).count();
    }
};

#endif /* UTIL_HPP_ */
