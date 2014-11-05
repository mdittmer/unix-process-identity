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

#include <sys/types.h>

typedef int FileDescriptor;

template<typename Functor>
class Fork {
public:
    Fork(Functor f) :
        functor(f),
        fdRead(-1),
        hasRun(false),
        hasRead(false),
        readValue() {}

    FileDescriptor run(typename Functor::Param const& p);

    typename Functor::Rtn read();

    int wait();

    typename Functor::Rtn operator() (typename Functor::Param const& p);

private:
    Functor functor;
    FileDescriptor fdRead;
    bool hasRun;
    bool hasRead;
    typename Functor::Rtn readValue;
    pid_t childPID;
};

// Fork functors should typedef Rtn and Param appropriately
#define FORK_FUNCTOR_OPERATOR Rtn operator() (Param const& param);

#include "ForkImpl.h"
