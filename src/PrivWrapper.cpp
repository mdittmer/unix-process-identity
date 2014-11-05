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


#include "PrivWrapper.h"

#include "Assertions.h"
#include "SetuidState.h"

extern "C" {
#include "priv.h"

#include <stdlib.h>
}

// External variable: "uid" parameter
#define PRIV_IMPL(_name) {                      \
        pcred_t pcred;                          \
        ucred_t ucred;                          \
        prepCredentials(pcred, ucred, uid);     \
        int rtn = _name(&ucred);                \
        cleanupCredentials(pcred, ucred, uid);  \
        return rtn;                             \
    }


int PrivWrapper::dropPrivPerm(UID uid) PRIV_IMPL(drop_privileges_permanently);
int PrivWrapper::dropPrivTemp(UID uid) PRIV_IMPL(drop_privileges_temporarily);
int PrivWrapper::restorePriv(UID uid) PRIV_IMPL(restore_privileges);

void PrivWrapper::prepCredentials(
    pcred_t& pcred,
    ucred_t& ucred,
    UID uid) {
    ASSERT(!get_pcred(&pcred));
    ucred.uid = uid;
    ucred.gid = pcred.gids.e;
    ucred.sups = pcred.sups;
}

void PrivWrapper::cleanupCredentials(
    pcred_t& pcred,
    ucred_t& ucred,
    UID uid) {
    free(pcred.sups.list);
}
