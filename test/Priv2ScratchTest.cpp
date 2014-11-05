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
// Create an executable that can quickly look up particulars during debugging

extern "C" {
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#include "priv.h"
#include "priv2.h"
#include "priv2_generated.h"
}

#include "Assertions.h"

int main() {
    if (geteuid() != 0) {
        printf("ERROR: Test is to be run with euid=0\n");
        return -1;
    }
    ucred_t ucred;
    ucred.uid = 0;
    // ucred.uids.e = 0;
    // ucred.uids.s = 0;
    ucred.gid = 0;
    // ucred.gids.e = 0;
    // ucred.gids.s = 0;
    ASSERT(get_sups(&ucred.sups) == 0);

    ASSERT(drop_privileges_temporarily(&ucred) == 0 && geteuid() == 0);
    ASSERT(drop_privileges_permanently(&ucred) == 0 && geteuid() == 0);

    // printf("%d\n", predecessor_matrix[i][j]);
    return 0;
}
