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


extern "C" {
#include <grp.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include "priv.h"
}

#include <iostream>

std::ostream& operator<<(std::ostream& os, sups_t const& sups) {
    os << "[ ";
    for (int i = 0; i < sups.size; ++i) {
        os << sups.list[i] << " ";
    }
    os << "]";
    return os;
}

int get_raw_sups(sups_t* sups) {
    int size = getgroups(0, NULL);
    if (size < 0) {
        return -1;
    }
    gid_t* list = (gid_t*)(malloc(sizeof(gid_t) * size));
    if (getgroups(size, list) < 0) {
        return -1;
    }
    sups->size = size;
    sups->list = list;
    return 0;
}

int sups_idx(sups_t const* sups, gid_t gid) {
    for (int i = 0; i < sups->size; ++i) {
        if (sups->list[i] == gid) {
            return i;
        }
    }
    return -1;
}

bool sups_contains(sups_t const* sups, gid_t gid) {
    return (sups_idx(sups, gid) >= 0);
}

int test_sups(
    gid_t test_gid,
    unsigned& sups_contains_count,
    unsigned& sups_at_zero_count,
    unsigned& test_count) {
    sups_t get_sups;
    if (get_raw_sups(&get_sups) != 0) {
        std::cerr << "ERROR: Failed to get sups" << std::endl;
        return -1;
    }
    if (sups_contains(&get_sups, test_gid)) {
        std::cout << "GID " << test_gid << " added to sups  " << get_sups << std::endl;
        ++sups_contains_count;
    }
    if (sups_idx(&get_sups, test_gid) == 0) {
        std::cout << "GID " << test_gid << " added to sups at [0]  " << get_sups << std::endl;
        ++sups_at_zero_count;
    }
    ++test_count;
    free(get_sups.list);
    return 0;
}

int main() {
    if (geteuid() != 0 || getegid() != 0) {
        std::cerr << "ERROR: Test must be run as root with egid=0" << std::endl;
        return -1;
    }
    gid_t sups_list[4] = {
        0, // On some systems this MUST be the current egid
        10,
        11,
        12 };
    sups_t sups; sups.size = sizeof(sups_list) / sizeof(gid_t); sups.list = sups_list;
    unsigned sups_contains_count = 0, sups_at_zero_count = 0, test_count = 0;

    if (setgroups(sups.size, sups.list) != 0) {
        std::cerr << "ERROR: Failed to set sups" << std::endl;
        return -1;
    }

    // Do not test when egid = 0 (original egid); could get false results
    // if (test_sups(0, sups_contains_count, sups_at_zero_count, test_count) != 0) {
    //     std::cerr << "ERROR: Testing sups failed" << std::endl;
    //     return -1;
    // }

    if (setegid(1) != 0) {
        std::cerr << "ERROR: Failed temporarily set egid" << std::endl;
        return -1;
    }

    if (test_sups(1, sups_contains_count, sups_at_zero_count, test_count) != 0) {
        std::cerr << "ERROR: Testing sups failed" << std::endl;
        return -1;
    }

    if (setegid(0) != 0) {
        std::cerr << "ERROR: Failed restore egid" << std::endl;
        return -1;
    }

    // Do not test when egid = 0 (original egid); could get false results
    // if (test_sups(0, sups_contains_count, sups_at_zero_count, test_count) != 0) {
    //     std::cerr << "ERROR: Testing sups failed" << std::endl;
    //     return -1;
    // }

    if (setregid(20, 30) != 0) {
        std::cerr << "ERROR: Failed set real and effective gid" << std::endl;
        return -1;
    }

    if (test_sups(30, sups_contains_count, sups_at_zero_count, test_count) != 0) {
        std::cerr << "ERROR: Testing sups failed" << std::endl;
        return -1;
    }

    if (sups_contains_count > 0 && sups_contains_count < test_count) {
        std::cerr << "ERROR: egid is sometimes added to getgroups() list, but not always" << std::endl;
        return -1;
    }
    if (sups_contains_count == test_count) {
        std::cerr << "NOTE: egid is always added to getgroups() list" << std::endl;
    }
    if (sups_contains_count == 0) {
        std::cerr << "NOTE: egid is never added to getgroups() list" << std::endl;
    }
    if (sups_at_zero_count == test_count) {
        std::cerr << "NOTE: egid is always at [0] in getgroups() list" << std::endl;
    }
    if (sups_at_zero_count == 0) {
        std::cerr << "NOTE: egid is never at [0] in getgroups() list" << std::endl;
    }

    return 0;
}
