// Copyright (c) 2014 Mark S. Dittmer
//
// This file is part of Priv2.
//
// Priv2 is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Priv2 is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Priv2.  If not, see <http://www.gnu.org/licenses/>.


#include "Assertions.h"

#include <boost/regex.hpp>

#include <errno.h>

#include <cstdlib>
#include <climits>
#include <stdexcept>

using namespace boost;

int stoi(char const* str, char** end, int base) {
    static regex zero("^[\\h\\v]*[+-]?0*");
    static regex nonZero("^[\\h\\v]*[+-]?0*[1-9]+");
    static regex integer("^[\\h\\v]*[+-]?[0-9]+");

    // Must be integer
    if (!regex_match(str, integer)) {
        throw std::invalid_argument(str);
    }
    long l = std::strtol(str, end, base);
    int i = static_cast<int>(l);
    // Catch case where 0 is returned, but real value is not 0
    if (l == 0L &&
        !(regex_match(str, zero) && !regex_match(str, nonZero))) {
        throw std::invalid_argument(str);
    }
    // Catch case where value is bigger than a long
    if ((l == LONG_MAX || l == LONG_MIN) && errno == ERANGE) {
        throw std::out_of_range(str);
    }
    // Catch case where value is bigger than an int
    if (static_cast<long>(i) != l) {
        throw std::out_of_range(str);
    }
    return i;
}
