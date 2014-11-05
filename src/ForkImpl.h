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
#include "SetuidState.h"

#include <boost/iostreams/device/file_descriptor.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/set.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/vector.hpp>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace ba = boost::archive;
namespace bios = boost::iostreams;

template <typename Functor>
FileDescriptor Fork<Functor>::run(typename Functor::Param const& p) {
    if (hasRun) {
        return fdRead;
    }

    FileDescriptor fd[2];

    int pipeRtn = pipe(fd);

    if (pipeRtn == -1) {
        return -1;
    }

    childPID = fork();

    if (childPID == -1) {
        ASSERT(close(fd[0]) == 0);
        ASSERT(close(fd[1]) == 0);
        return -1;
    }

    hasRun = true;
    fdRead = fd[0];

    if (childPID == 0) {
        // Child
        ASSERT(close(fd[0]) == 0);
        bios::stream<bios::file_descriptor_sink> fdos(
            fd[1],
            bios::close_handle); // TODO: Should close this?
        {
            ba::text_oarchive oa(fdos);
            typename Functor::Rtn const rtn = functor(p);
            oa << rtn;
        }
        exit(0);
    } else {
        // Parent
        ASSERT(close(fd[1]) == 0);
    }

    return fdRead;
}

template <typename Functor>
typename Functor::Rtn Fork<Functor>::read() {
    if (hasRead) {
        return readValue;
    }
    bios::stream<bios::file_descriptor_source> fdis(
        fdRead,
        bios::close_handle); // TODO: Should close this?
    {
        ba::text_iarchive ia(fdis);
        ia >> readValue;

        hasRead = true;
    }
    return readValue;
}

template <typename Functor>
int Fork<Functor>::wait() {
    int rtn;
    ASSERT(childPID > 0);
    waitpid(childPID, &rtn, 0);
    return rtn;
}

template <typename Functor>
typename Functor::Rtn Fork<Functor>::operator() (typename Functor::Param const& p) {
    run(p);
    return read();
}
