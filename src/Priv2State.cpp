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


#include "Priv2State.h"

#include "Assertions.h"

extern "C" {
#include "priv.h"
#include "priv2.h"
}

SetgidState SetgidState::get() {
    GID rgid, egid, svgid;

#if HAS_SETRESUID

    ASSERT(!getresgid(&rgid, &egid, &svgid));

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

    rgid = credentials.pr_rgid;
    egid = credentials.pr_egid;
    svgid = credentials.pr_sgid;

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

    rgid = info.pbsi_rgid;
    egid = info.pbsi_gid;
    svgid = info.pbsi_svgid;

#endif

#endif

    return SetgidState(rgid, egid, svgid);
}
std::ostream& operator<<(std::ostream& os, SetgidState const& ss) {
    // Cast to signed ints for easy readability of negative value tests
    os << "<rg:" << static_cast<int>(ss.rgid)
       << ", eg:" << static_cast<int>(ss.egid)
       << ", svg:" << static_cast<int>(ss.svgid) << ">";
    return os;
}

// TODO: "Current euid caveat" from Tsafrir & Wagner '08 is not managed properly
// here; we would need to implement it in the context of a full process state

bool SupGroups::operator<(SupGroups const& sg) const {
    return list < sg.list;
}

bool SupGroups::operator==(SupGroups const& sg) const {
    return list == sg.list;
}

std::ostream& operator<<(std::ostream& os, SupGroups const& sg) {
    os << "[";
    for (unsigned i = 0; i < sg.size(); ++i) {
        // Cast to signed ints for easy readability of negative value tests
        os << static_cast<int>(sg.list[i]);
        if (i + 1 < sg.size()) {
            os << ", ";
        }
    }
    os << "]";
    return os;
}

Priv2State Priv2State::get() {
    Priv2State s;
    s.uState = SetuidState::get();
    s.gState = SetgidState::get();
    s.supGroups = SupGroups::get();
    return s;
}

std::ostream& operator<<(std::ostream& os, Priv2State const& s) {
    os << "{ " << s.uState << ", " << s.gState << ", " << s.supGroups << " }";
    return os;
}

bool Priv2FunctionParam::operator<(Priv2FunctionParam const& p) const {
    return (uid < p.uid || gid < p.gid || supGroups < p.supGroups);
}

bool Priv2FunctionParam::operator==(Priv2FunctionParam const& p) const {
    return (uid == p.uid && gid == p.gid && supGroups == p.supGroups);
}

std::ostream& operator<<(std::ostream& os, Priv2Function const& p2f) {
    switch (p2f) {
    default:
        ASSERT(false);
        break;
    case AssumeIDPerm:
        os << "change_identity_permanently";
        break;
    case AssumeIDTemp:
        os << "change_identity_temporarily";
        break;
    }
    return os;
}

std::ostream& operator<<(std::ostream& os, IDFunction const& idf) {
    switch(idf.ty) {
    default:
        ASSERT(false);
        break;
    case SetuidFunctionType:
        os << idf.fn.setuid;
        break;
    case Priv2FunctionType:
        os << idf.fn.priv2;
        break;
    }
    return os;
}

std::ostream& operator<<(std::ostream& os, IDFunctionParams const& idfp) {
    switch(idfp.ty) {
    default:
        ASSERT(false);
        break;
    case SetuidFunctionType:
        os << idfp.setuidParams;
        break;
    case Priv2FunctionType:
        os << idfp.priv2Param;
        break;
    }
    return os;
}

std::ostream& operator<<(std::ostream& os, Priv2Call const& p2c) {
    ASSERT(p2c.params.size() == 1);
    os << p2c.function << "(" << p2c.params.at(0) << ")";
    return os;
}

std::ostream& operator<<(std::ostream& os, Priv2FunctionCall const& p2fc) {
    ASSERT(p2fc.params.size() == 1);
    os << p2fc.function << "(" << p2fc.params.at(0) << ") : " << p2fc.rtn;
    return os;
}
