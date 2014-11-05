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
// priv2_setuid.h
//     Based on the work of Dan Tsafrir and David Wagner
//     Subsequent work by Mark Dittmer
//
// See also
//     http://code.google.com/p/change-process-identity
//     http://www.usenix.org/publications/login/2008-06/pdfs/tsafrir.pdf
//##############################################################################

#include <stdlib.h>
#include <sys/types.h>
#ifdef MULTITHREADED
#include <pthread.h>
#endif

#include "priv2_setuid.h"

#include "priv.h"

#ifdef MULTITHREADED
static pthread_mutex_t setuid_mutex;

int change_uid_pthread_setup() {
    return pthread_mutex_init(&setuid_mutex, NULL);
}

int change_uid_pthread_teardown() {
    return pthread_mutex_destroy(&setuid_mutex);
}
#endif

static void fill_ucred(uid_t const uid, ucred_t* ucred) {
    pcred_t pcred;
    get_pcred(&pcred);
    ucred->uid = uid;

    // Leave groups as-is.
    ucred->gid = pcred.gids.e;
    // ucred takes ownership of malloc'd pcred.sups.list pointer.
    ucred->sups = pcred.sups;
}

static int change_uid(uid_t const uid, bool is_permanent) {
    ucred_t ucred;
    fill_ucred(uid, &ucred);

#ifdef MULTITHREADED
    pthread_mutex_lock(&setuid_mutex);
#endif

    int rtn = is_permanent ?
        change_identity_permanently(&ucred) :
        change_identity_temporarily(&ucred);

#ifdef MULTITHREADED
    pthread_mutex_unlock(&setuid_mutex);
#endif

    // Free memory malloc'd by get_pcred() (via fill_ucred()).
    free(ucred.sups.list);

    return rtn;
}

int change_uid_permanently(uid_t const uid) {
    return change_uid(uid, true);
}

int change_uid_temporarily(uid_t const uid) {
    return change_uid(uid, false);
}

//##############################################################################
//                                   EOF
//##############################################################################
