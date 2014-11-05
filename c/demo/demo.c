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
//##############################################################################
// priv2 demo
//     Based on the work of Dan Tsafrir and David Wagner
//     Subsequent work by Mark Dittmer
//
// See also
//     http://code.google.com/p/change-process-identity
//     http://www.usenix.org/publications/login/2008-06/pdfs/tsafrir.pdf
//##############################################################################

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include "priv2_setuid.h"

int main() {
    printf("Starting euid: %d\n\n", geteuid());
    if (change_uid_temporarily(1) != 0) {
        assert(errno == EPERM);
        printf("FAILED: change_uid_temporarily(1)\n"
               "This is expected for a non-root setuid process started by\n"
               "any user OTHER than uid=1\n\n");
        return 0;
    }
    if (change_uid_permanently(2) != 0) {
        assert(errno == EPERM);
        printf("FAILED: change_uid_permanently(2)\n"
               "This is expected for a non-root setuid process started by\n"
               "uid=1\n\n");
        return 0;
    }
    printf("SUCCEEDED: change_uid_temporarily(1),\n"
           "            change_uid_permanently(2)\n"
           "This is expected for:\n"
           "(1) a root-setuid process\n"
           "(2) a uid=1-setuid process\n"
           "(3) a uid=2-setuid process\n\n");
    return 0;
}
