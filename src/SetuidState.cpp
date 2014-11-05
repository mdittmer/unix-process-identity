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


#include "SetuidState.h"

#include "Assertions.h"
#include "Platform.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>
#include <fstream>
#include <string>

#if IS_SOLARIS

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <procfs.h>

#endif

#if !HAS_SETRESUID

#include <sstream>

#if !IS_SOLARIS

// BSD and BSD-like implementations have C API to procfs or procfs-like
// structures
#include <libproc.h>
#include <sys/proc_info.h>

#endif

#endif


std::ostream& operator<<(std::ostream& os, SetuidFunction const& sf) {
    switch (sf) {
    default:
        ASSERT(false);
        break;
    case Setuid:
        os << "setuid";
        break;
    case Seteuid:
        os << "seteuid";
        break;
    case Setreuid:
        os << "setreuid";
        break;
    case Setresuid:
        os << "setresuid";
        break;
    case DropPrivPerm:
        os << "dropprivperm";
        break;
    case DropPrivTemp:
        os << "dropprivtemp";
        break;
    case RestorePriv:
        os << "restorepriv";
        break;
    }
    return os;
}

SetuidState SetuidState::get() {
    UID ruid, euid, svuid;

#if HAS_SETRESUID

    ASSERT(!getresuid(&ruid, &euid, &svuid));

#else

    pid_t pid = getpid();

#if IS_SOLARIS

    // TODO: This procedure has not been verified!
    std::stringstream filename;
    filename << "/proc/" << pid << "/cred";
    prcred_t credentials;
    int credFile = open(filename.str().c_str(), O_RDONLY);
    ASSERT(credFile >= 0);
    int bytesRead = read(credFile, &credentials, sizeof(prcred_t));
    ASSERT(bytesRead == sizeof(prcred_t));
    close(credFile);

    ruid = credentials.pr_ruid;
    euid = credentials.pr_euid;
    svuid = credentials.pr_suid;

#else

    // TODO: This procedure has not been verified on all non-Solaris, non-getresuid systems
    struct proc_bsdshortinfo info;
    int syscallRtn = proc_pidinfo(
        pid,
        PROC_PIDT_SHORTBSDINFO,
        0,
        &info,
        sizeof(info));
    ASSERT(syscallRtn == sizeof(info));

    ruid = info.pbsi_ruid;
    euid = info.pbsi_uid;
    svuid = info.pbsi_svuid;

#endif

#endif

    return SetuidState(ruid, euid, svuid);
}

std::ostream& operator<<(std::ostream& os, SetuidState const& ss) {
    // Cast to signed ints for easy readability of negative value tests
    os << "<ru:" << static_cast<int>(ss.ruid)
       << ", eu:" << static_cast<int>(ss.euid)
       << ", svu:" << static_cast<int>(ss.svuid) << ">";
    return os;
}

std::ostream& operator<<(std::ostream& os, SetuidFunctionReturn const& sfr) {
    os << ": " << sfr.value;
    if (sfr.value != 0) {
        os << " !";
        switch (sfr.errNumber) {
        default:
            os << sfr.errNumber;
            break;
        case EINVAL:
            os << "EINVAL";
            break;
        case EPERM:
            os << "EPERM";
            break;
        }
        // os << " " << sfr.errDescription;
    }
    return os;
}

std::ostream& operator<<(std::ostream& os, SetuidFunctionParams const& sfp) {
    os << "(";
    unsigned count = 0;
    for (SetuidFunctionParams::const_iterator it = sfp.begin(), ie = sfp.end(); it != ie; ++it) {
        os << *it;
        if (it + 1 != ie) {
            os << ", ";
        }
        ++count;
    }
    os << ")";
    return os;
}

std::ostream& operator<<(std::ostream& os, SetuidFunctionCall const& sfc) {
    os << sfc.function << sfc.params << sfc.rtn;
    return os;
}
