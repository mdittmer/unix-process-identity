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


#include "Platform.h"
#include "SetuidState.h"
#include "Util.h"

#include <unistd.h>

#include <iostream>

static uid_t puid;
static uid_t uuid;

static void becomeProcess() {
#if HAS_SETRESUID

    setresuid(uuid, puid, puid);

#else

    setreuid(uuid, puid);

#endif
}

static void becomeUser() {
#if HAS_SETRESUID

    setresuid(uuid, uuid, puid);

#else

    setreuid(puid, uuid);

#endif
}

int main(int argc, char* argv[]) {
    SetuidState s = SetuidState::get();
    // if (s.euid != 0) {
    //     std::cerr << "ERROR: Process must start as root" << std::endl;
    //     return -1;
    // }
    if (argc != 2) {
        std::cerr << "ERROR: Expected exactly one argument: process user ID" << std::endl;
        return -1;
    }
    puid = static_cast<uid_t>(stoi(argv[1], NULL, 10));
    if (puid == static_cast<uid_t>(-1)) {
        std::cerr << "ERROR: Process UID cannot be -1" << std::endl;
        return -1;
    }

    uuid = getuid();

    becomeProcess();

    // Do stuff "as the process"...

    s = SetuidState::get();
    if (s.euid != puid) {
        std::cerr << "ERROR: Failed to assume process identity" << std::endl;
        return -1;
    }
    if (s.ruid == 0 || s.euid == 0 || s.svuid == 0) {
        std::cerr << "ERROR: Failed remove root privileges while assuming process identity" << std::endl;
        return -1;
    }

    becomeUser();

    // Do stuff "as the user"...

    s = SetuidState::get();
    if (s.euid != uuid) {
        std::cerr << "ERROR: Failed to assume user identity" << std::endl;
        return -1;
    }
    if (s.ruid == 0 || s.euid == 0 || s.svuid == 0) {
        std::cerr << "ERROR: Failed remove root privileges while assuming user identity" << std::endl;
        return -1;
    }

    return 0;
}
