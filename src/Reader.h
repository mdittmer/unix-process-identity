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
#pragma once

#include "Assertions.h"

#include <iostream>
#include <string>

template<typename T>
class ReadWriteParam {
public:
    ReadWriteParam(std::string const& _tag, T& _param) :
        tag(_tag), param(_param) {}

    friend std::istream &operator>>(std::istream& is, ReadWriteParam& wrp) {
        int size = wrp.tag.size() + 1;
        char* buffer = new char[size];
        buffer[size - 1] = '\0';
        is.read(buffer, size - 1);
        std::cout << "GCOUNT: " << is.gcount() << std::endl;
        ASSERT(is.gcount() == size - 1);
        std::string tagRead(buffer);
        ASSERT(tagRead == wrp.tag);
        delete[] buffer;
        is >> wrp.param;
        return is;
    }

    friend std::ostream &operator<<(std::ostream& os, ReadWriteParam const& wrp) {
        std::cout << wrp.tag << std::endl;
        std::cout << wrp.param << std::endl;
        // os << wrp.tag << wrp.param << std::endl;
        return os;
    }

    T& operator*() { return param; }
    T& operator->() { return param; }

    bool operator<(ReadWriteParam const& other) const {
        return param < other.param;
    }
    bool operator==(ReadWriteParam const& other) const {
        return param == other.param;
    }

    bool operator<(T const& other) const {
        return param < other;
    }
    bool operator==(T const& other) const {
        return param == other;
    }

private:
    std::string const tag;
    T param;
};
