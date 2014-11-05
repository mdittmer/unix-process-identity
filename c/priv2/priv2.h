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
// priv2.h
//     Based on the work of Dan Tsafrir and David Wagner
//     Subsequent work by Mark Dittmer
//
// See also
//     http://code.google.com/p/change-process-identity
//     http://www.usenix.org/publications/login/2008-06/pdfs/tsafrir.pdf
//##############################################################################

#ifndef PRIV2_H__
#define PRIV2_H__

#include "priv.h"


typedef struct process_credential_pair {
    pcred_t prev;
    pcred_t next;
} pcred_pair_t;

#define ID_PAIR_COLLECTION_SIZE 5
typedef struct id_pair_collection {
    pcred_t prev;
    uids_t uids[ID_PAIR_COLLECTION_SIZE];
    gids_t gids[ID_PAIR_COLLECTION_SIZE];
    sups_t sups;
} id_pairs_t;


// Permanently change identity of a process to that of the given user.
int change_identity_permanently(ucred_t const* uc);


// Switch to the given user, but maintain the ability to regain the
// current effective identity.
int change_identity_temporarily(ucred_t const* uc);

#endif /* PRIV2_H__ */

//##############################################################################
//                                   EOF
//##############################################################################
