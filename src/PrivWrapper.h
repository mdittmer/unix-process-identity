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

#include "SetuidState.h"

extern "C" {
#include "priv.h"
}

struct PrivWrapper {
    static int dropPrivPerm(UID uid);
    static int dropPrivTemp(UID uid);
    static int restorePriv(UID uid);

private:
    static void prepCredentials(pcred_t& pcred, ucred_t& ucred, UID uid);
    static void cleanupCredentials(pcred_t& pcred, ucred_t& ucred, UID uid);
};
