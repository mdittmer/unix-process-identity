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

#include "Assertions.h"
#include "Graph.h"

#include <errno.h>

#define P2U(param) static_cast<UID>(param)

template<typename Graph>
class APFunctor {
public:
    typedef typename Graph::Vertex Vertex;
    typedef typename Graph::VertexPropertyType VertexProperty;
    typedef typename Graph::EdgePropertyType EdgeProperty;

    virtual ~APFunctor() {}

    virtual bool operator() (
        EdgeProperty const& fc,
        VertexProperty const& s1,
        VertexProperty const& s2) const = 0;
};

#define APF_CLASSNAME(_name) \
    template<typename Graph>  \
    class _name : public APFunctor< Graph >

#define APF_OP                                  \
    virtual bool operator() (                   \
        EdgeProperty const& fc,                 \
        VertexProperty const& s1,               \
        VertexProperty const& s2) const

#define APF_HEADER(_name)                                               \
    public:                                                             \
    typedef typename Graph::Vertex Vertex;                              \
    typedef typename Graph::VertexPropertyType VertexProperty;          \
    typedef typename Graph::EdgePropertyType EdgeProperty;              \
                                                                        \
    virtual ~_name() {}

#define APF_START(_name)                        \
    APF_CLASSNAME(_name) {                      \
    APF_HEADER(_name)                           \
    APF_OP

#define APF_FINISH };

#define APF_TYPE(_class_name) _class_name< Graph >
#define CONS_APF(_class_name) APF_TYPE(_class_name)()
#define CONS_APF_CONST(_class_name, _name) _name(CONS_APF(_class_name))
#define APF_CONST(_class_name, _name) APF_TYPE(_class_name) const _name

// "Value" or "eVal"...
// NOTE: Depends on functor input names: fc, s1, s2
#define V(_functor_name) _functor_name(fc, s1, s2)

APF_START(Fail) {
    return (fc.rtn.value == -1 &&
            s1.ruid == s2.ruid &&
            s1.euid == s2.euid &&
            s1.svuid == s2.svuid);
} APF_FINISH

// TODO: Should we be asserting a particular EINVAL meaning rather than checking
// the system behaviour?
APF_START(Einval) {
    return (fc.rtn.value == -1 &&
            fc.rtn.errNumber == EINVAL);
} APF_FINISH

APF_CLASSNAME(SuccessXorFail) {
    APF_HEADER(SuccessXorFail);
    APF_OP {
        return ((V(*success) && !V(fail)) ||
                (!V(*success) && V(fail)));
    }
public:
    SuccessXorFail(APF_TYPE(APFunctor)* _success) :
        success(_success),
        CONS_APF_CONST(Fail, fail) {
        ASSERT(_success);
    }
private:
    APF_TYPE(APFunctor)* success;
    APF_CONST(Fail, fail);
};

// Setuid

APF_START(SetuidSuccessAP) {
    return (fc.rtn.value == 0 &&
            s2.ruid == P2U(fc.params.at(0)) &&
            s2.euid == P2U(fc.params.at(0)) &&
            s2.svuid == P2U(fc.params.at(0)));
} APF_FINISH

APF_START(SetuidSuccessNAP) {
    return (fc.rtn.value == 0 &&
            s2.ruid == s1.ruid &&
            s2.euid == P2U(fc.params.at(0)) &&
            s2.svuid == s1.svuid);
} APF_FINISH

APF_CLASSNAME(SetuidSuccess) {
    APF_HEADER(SetuidSuccess);
    APF_OP {
        return (V(successAP) || V(successNAP));
    }
public:
    SetuidSuccess() :
        CONS_APF_CONST(SetuidSuccessAP, successAP),
        CONS_APF_CONST(SetuidSuccessNAP, successNAP) {}
private:
    APF_CONST(SetuidSuccessAP, successAP);
    APF_CONST(SetuidSuccessNAP, successNAP);
};

APF_START(SetuidNAPUIDIsValid) {
    return (s1.ruid == P2U(fc.params.at(0)) ||
            s1.svuid == P2U(fc.params.at(0)));
} APF_FINISH

APF_CLASSNAME(SetuidImpliesAP) {
    APF_HEADER(SetuidImpliesAP);
    APF_OP {
        return ((V(napUIDIsValid) && !V(successNAP)) ||
                (!V(napUIDIsValid) && !V(einval) && !V(fail)));
    }
public:
    SetuidImpliesAP() :
        CONS_APF_CONST(SetuidSuccessNAP, successNAP),
        CONS_APF_CONST(SetuidNAPUIDIsValid, napUIDIsValid),
        CONS_APF_CONST(Einval, einval),
        CONS_APF_CONST(Fail, fail) {}
private:
    APF_CONST(SetuidSuccessNAP, successNAP);
    APF_CONST(SetuidNAPUIDIsValid, napUIDIsValid);
    APF_CONST(Einval, einval);
    APF_CONST(Fail, fail);
};

APF_CLASSNAME(SetuidImpliesNAP) {
    APF_HEADER(SetuidImpliesNAP);
    APF_OP {
        return ((!V(successAP) && !V(einval)) ||
                (!V(napUIDIsValid) && !V(einval) && V(fail)));
    }
public:
    SetuidImpliesNAP() :
        CONS_APF_CONST(SetuidSuccessAP, successAP),
        CONS_APF_CONST(SetuidNAPUIDIsValid, napUIDIsValid),
        CONS_APF_CONST(Einval, einval),
        CONS_APF_CONST(Fail, fail) {}
private:
    APF_CONST(SetuidSuccessAP, successAP);
    APF_CONST(SetuidNAPUIDIsValid, napUIDIsValid);
    APF_CONST(Einval, einval);
    APF_CONST(Fail, fail);
};

// setuid(): !(AP && !AP) = true
APF_CLASSNAME(SetuidTautology) {
    APF_HEADER(SetuidTautology);
    APF_OP {
        return !(V(impliesAP) && V(impliesNAP));
    }
public:
    SetuidTautology() :
        CONS_APF_CONST(SetuidImpliesAP, impliesAP),
        CONS_APF_CONST(SetuidImpliesNAP, impliesNAP) {}
private:
    APF_CONST(SetuidImpliesAP, impliesAP);
    APF_CONST(SetuidImpliesNAP, impliesNAP);
};

APF_CLASSNAME(SetuidRootAP) {
    APF_HEADER(SetuidRootAP);
    APF_OP {
        return (s1.euid != 0 || // Only check root
                !V(impliesNAP));
    }
public:
    SetuidRootAP() :
        CONS_APF_CONST(SetuidImpliesNAP, impliesNAP) {}
private:
    APF_CONST(SetuidImpliesNAP, impliesNAP);
};

APF_CLASSNAME(SetuidNonRootNAP) {
    APF_HEADER(SetuidNonRootNAP);
    APF_OP {
        return (s1.euid == 0 || // Only check non-root
                !V(impliesAP));
    }
public:
    SetuidNonRootNAP() :
        CONS_APF_CONST(SetuidImpliesAP, impliesAP) {}
private:
    APF_CONST(SetuidImpliesAP, impliesAP);
};

// Seteuid

APF_START(SeteuidSuccess) {
    return (fc.rtn.value == 0 &&
            s2.ruid == s1.ruid &&
            s2.euid == P2U(fc.params.at(0)) &&
            s2.svuid == s1.svuid);
} APF_FINISH

// NOTE: SetuidNAPUIDIsValid applies equally to seteuid
#define SeteuidNAPUIDIsValid SetuidNAPUIDIsValid

APF_CLASSNAME(SeteuidImpliesAP) {
    APF_HEADER(SeteuidImpliesAP);
    APF_OP {
        return ((V(napUIDIsValid) && !V(success)) ||
                (!V(napUIDIsValid) && !V(einval) && !V(fail)));
    }
public:
    SeteuidImpliesAP() :
        CONS_APF_CONST(SeteuidSuccess, success),
        CONS_APF_CONST(SeteuidNAPUIDIsValid, napUIDIsValid),
        CONS_APF_CONST(Einval, einval),
        CONS_APF_CONST(Fail, fail) {}
private:
    APF_CONST(SeteuidSuccess, success);
    APF_CONST(SeteuidNAPUIDIsValid, napUIDIsValid);
    APF_CONST(Einval, einval);
    APF_CONST(Fail, fail);
};

APF_CLASSNAME(SeteuidImpliesNAP) {
    APF_HEADER(SeteuidImpliesNAP);
    APF_OP {
        return ((!V(success) && !V(einval)) ||
                (!V(napUIDIsValid) && !V(einval) && V(fail)));
    }
public:
    SeteuidImpliesNAP() :
        CONS_APF_CONST(SeteuidSuccess, success),
        CONS_APF_CONST(SeteuidNAPUIDIsValid, napUIDIsValid),
        CONS_APF_CONST(Einval, einval),
        CONS_APF_CONST(Fail, fail) {}
private:
    APF_CONST(SeteuidSuccess, success);
    APF_CONST(SeteuidNAPUIDIsValid, napUIDIsValid);
    APF_CONST(Einval, einval);
    APF_CONST(Fail, fail);
};

// seteuid(): !(AP && !AP) = true
APF_CLASSNAME(SeteuidTautology) {
    APF_HEADER(SeteuidTautology);
    APF_OP {
        return !(V(impliesAP) && V(impliesNAP));
    }
public:
    SeteuidTautology() :
        CONS_APF_CONST(SeteuidImpliesAP, impliesAP),
        CONS_APF_CONST(SeteuidImpliesNAP, impliesNAP) {}
private:
    APF_CONST(SeteuidImpliesAP, impliesAP);
    APF_CONST(SeteuidImpliesNAP, impliesNAP);
};

APF_CLASSNAME(SeteuidRootAP) {
    APF_HEADER(SeteuidRootAP);
    APF_OP {
        return (s1.euid != 0 || // Only check root
                !V(impliesNAP));
    }
public:
    SeteuidRootAP() :
        CONS_APF_CONST(SeteuidImpliesNAP, impliesNAP) {}
private:
    APF_CONST(SeteuidImpliesNAP, impliesNAP);
};

APF_CLASSNAME(SeteuidNonRootNAP) {
    APF_HEADER(SeteuidNonRootNAP);
    APF_OP {
        return (s1.euid == 0 || // Only check non-root
                !V(impliesAP));
    }
public:
    SeteuidNonRootNAP() :
        CONS_APF_CONST(SeteuidImpliesAP, impliesAP) {}
private:
    APF_CONST(SeteuidImpliesAP, impliesAP);
};

// Setreuid

// NOTE: setreuid() definition is inconsistent. Here, we make some
// assumptions that are empirically advantageous for setreuid()

// ASSUMPTIONS:
// (1) Disambiguate: "real user ID" (without "current" or "new") refers to the
//     CURRENT real user ID
// (2) Disambiguate: "either the real, effective, or saved user ID of the
//     process" refers to the CURRENT [...] ID of the process
// (3) Disambiguate: "saved user ID" = "saved-set user ID"
//
// TODO: Pick one:
//
// (4a) Make sound: allow setting effective user ID to any of the three current
//      IDs (adopt language of DESCRIPTION section in standard)
// (4b) Make sound: allow setting effective user ID to current real or
//      saved-set user ID, but NOT current effective user ID (adopt language of
//      ERROR section in standard)
//
// REMOVED: (5) Make logical: Rewrite the rule ending in "then the saved set-user-ID of
//     the current process shall be set equal to the new effective user ID"
//     to read: "then the saved set-user-ID of the current process shall be set
//     equal to the OLD effective user ID" (emphasis added on the OLD)
// REMOVED: (6) Make logical: Only apply rule ending in "then the saved set-user-ID
//     of the current process shall be set equal to the [old] effective user ID"
//     when there is a new effective user ID (i.e., when new euid != 0)
// REMOVED: (7) Make complete: Adopt if-and-only-if on conditions when saved-set user ID
//     changes
// REMOVED: (8) Make complete: Allow unprivileged processes to set the real user ID to
//     the current real or effective user ID, but not the current saved
//     set-user-ID

// (9) Try: Interpret unqualified "real user ID" as param_ruid value

APF_START(SetreuidRuidSuccess) {
    return ((fc.params.at(0) == -1 && s2.ruid == s1.ruid) ||
            (fc.params.at(0) != -1 && s2.ruid == P2U(fc.params.at(0))));
} APF_FINISH

APF_START(SetreuidEuidSuccess) {
    return ((fc.params.at(1) == -1 && s2.euid == s1.euid) ||
            (fc.params.at(1) != -1 && s2.euid == P2U(fc.params.at(1))));
} APF_FINISH

APF_START(SetreuidSetSvuid) {
    return (// fc.params.at(1) != -1 && // REMOVED: Number (6) above
            // Cleaned up standard specification (contains redundant
            // "fc.params.at(1) != -1")
            (fc.params.at(0) != -1 ||
             (fc.params.at(1) != -1 &&
              // Without (9) above:
              P2U(fc.params.at(1)) != s1.ruid
              // With (9) above:
              // fc.params.at(1) != fc.params.at(0)
                 )));
} APF_FINISH

APF_CLASSNAME(SetreuidSvuidSuccess) {
    APF_HEADER(SetreuidSvuidSuccess);
    APF_OP {
        return (!V(setSvuid) || s2.svuid == s2.euid);
        // REMOVED: Number (7) above
        // return (( V(setSvuid) && s2.svuid == s2.euid) || // REMOVED: Number (5) above
        //         (!V(setSvuid) && s2.svuid == s1.svuid));
    }
public:
    SetreuidSvuidSuccess() :
        CONS_APF_CONST(SetreuidSetSvuid, setSvuid) {}
private:
    APF_CONST(SetreuidSetSvuid, setSvuid);
};

APF_CLASSNAME(SetreuidSuccess) {
    APF_HEADER(SetreuidSuccess);
    APF_OP {
        return (fc.rtn.value == 0 && V(ruidSuccess) &&
                V(euidSuccess) && V(svuidSuccess));
    }
public:
    SetreuidSuccess() :
        CONS_APF_CONST(SetreuidRuidSuccess, ruidSuccess),
        CONS_APF_CONST(SetreuidEuidSuccess, euidSuccess),
        CONS_APF_CONST(SetreuidSvuidSuccess, svuidSuccess) {}
private:
    APF_CONST(SetreuidRuidSuccess, ruidSuccess);
    APF_CONST(SetreuidEuidSuccess, euidSuccess);
    APF_CONST(SetreuidSvuidSuccess, svuidSuccess);
};

// This is currently dead code, because...
// REMOVED: Number (8) above
APF_START(SetreuidParamRuidIsValid) {
    return (fc.params.at(0) == -1 ||
            P2U(fc.params.at(0)) == s1.ruid ||
            P2U(fc.params.at(0)) == s1.euid);
} APF_FINISH

APF_START(SetreuidParamEuidIsValid) {
    return (fc.params.at(1) == -1 ||
            P2U(fc.params.at(1)) == s1.ruid ||
            P2U(fc.params.at(1)) == s1.euid ||
            P2U(fc.params.at(1)) == s1.svuid); // Number (4a) above
} APF_FINISH

// Commented out code below:
// This is what the standard says, but the standard does not hold that:
// SetreuidParamEuidIsInvalid() == !SetreuidParamEuidIsValid()
// For consistency (and according to assumption (4a) above), use only
// SetreuidParamEuidIsValid() (and !SetreuidParamEuidIsValid())

// APF_START(SetreuidParamEuidIsInvalid) {
//     return !(fc.params.at(1) == -1 ||
//              P2U(fc.params.at(1)) == s1.ruid ||
//              P2U(fc.params.at(1)) == s1.svuid);
// } APF_FINISH

APF_CLASSNAME(SetreuidImpliesAP) {
    APF_HEADER(SetreuidImpliesAP);
    APF_OP {
        // HACK: Implementation-dependent ruidIsValid must be true or false,
        // but not both. In order for AP to be implied, it must NECESSARILY
        // BE IMPLIED. Therefore, try expression with ruidIsValid = true and
        // ruidIsValid = false; only if both are true, AP is implied.
        bool ruidIsValid;

        ruidIsValid = true;
        bool trueExpr = ((!V(success) && ruidIsValid && V(euidIsValid)) ||
                (!V(fail) && !V(einval) &&
                 (!ruidIsValid || !V(euidIsValid))));

        ruidIsValid = false;
        bool falseExpr = ((!V(success) && ruidIsValid && V(euidIsValid)) ||
                (!V(fail) && !V(einval) &&
                 (!ruidIsValid || !V(euidIsValid))));

        return (trueExpr && falseExpr);

        // REMOVED: Number (8) above
        // return ((!V(success) && V(ruidIsValid) && V(euidIsValid)) ||
        //         (!V(fail) && !V(einval) &&
        //          (!V(ruidIsValid) || !V(euidIsValid))));
    }
public:
    SetreuidImpliesAP() :
        CONS_APF_CONST(SetreuidSuccess, success),
        // CONS_APF_CONST(SetreuidParamRuidIsValid, ruidIsValid),
        CONS_APF_CONST(SetreuidParamEuidIsValid, euidIsValid),
        CONS_APF_CONST(Einval, einval),
        CONS_APF_CONST(Fail, fail) {}
private:
    APF_CONST(SetreuidSuccess, success);
    // APF_CONST(SetreuidParamRuidIsValid, ruidIsValid);
    APF_CONST(SetreuidParamEuidIsValid, euidIsValid);
    APF_CONST(Einval, einval);
    APF_CONST(Fail, fail);
};

APF_CLASSNAME(SetreuidImpliesNAP) {
    APF_HEADER(SetreuidImpliesNAP);
    APF_OP {
        // HACK: Same hack as in SetreuidImpliesAP
        bool ruidIsValid;

        ruidIsValid = true;

        bool trueExpr = ((!V(success) && !V(einval)) ||
                         (V(fail) && !V(einval) &&
                          (!ruidIsValid || !V(euidIsValid))));

        ruidIsValid = false;
        bool falseExpr = ((!V(success) && !V(einval)) ||
                          (V(fail) && !V(einval) &&
                           (!ruidIsValid || !V(euidIsValid))));

        return (trueExpr && falseExpr);

        // REMOVED: Number (8) above
        // return ((!V(success) && !V(einval)) ||
        //         (V(fail) && !V(einval) &&
        //          (!V(ruidIsValid) || !V(euidIsValid))));
    }
public:
    SetreuidImpliesNAP() :
        CONS_APF_CONST(SetreuidSuccess, success),
        // CONS_APF_CONST(SetreuidParamRuidIsValid, ruidIsValid),
        CONS_APF_CONST(SetreuidParamEuidIsValid, euidIsValid),
        CONS_APF_CONST(Einval, einval),
        CONS_APF_CONST(Fail, fail) {}
private:
    APF_CONST(SetreuidSuccess, success);
    // APF_CONST(SetreuidParamRuidIsValid, ruidIsValid);
    APF_CONST(SetreuidParamEuidIsValid, euidIsValid);
    APF_CONST(Einval, einval);
    APF_CONST(Fail, fail);
};

// setuid(): !(AP && !AP) = true
APF_CLASSNAME(SetreuidTautology) {
    APF_HEADER(SetreuidTautology);
    APF_OP {
        // TODO: Just for testing
        bool const rtn = !(V(impliesAP) && V(impliesNAP));
        if (!rtn) {
            std::cerr << "Setreuid success: " << V(success) << std::endl;
            std::cerr << "Setreuid fail   : " << V(fail) << std::endl;
            std::cerr << "Setreuid einval : " << V(einval) << std::endl;
        }

        return !(V(impliesAP) && V(impliesNAP));
    }
public:
    SetreuidTautology() :
        CONS_APF_CONST(SetreuidImpliesAP, impliesAP),
        CONS_APF_CONST(SetreuidImpliesNAP, impliesNAP), // {}

        // TODO: Temporary for testing
        CONS_APF_CONST(SetreuidSuccess, success),
        CONS_APF_CONST(Einval, einval),
        CONS_APF_CONST(Fail, fail) {}
private:
    APF_CONST(SetreuidImpliesAP, impliesAP);
    APF_CONST(SetreuidImpliesNAP, impliesNAP);

    // TODO: Temporary for testing
    APF_CONST(SetreuidSuccess, success);
    APF_CONST(Einval, einval);
    APF_CONST(Fail, fail);
};

// Clean setreuid: bend the standard to eliminate violations

#define SetreuidCleanRuidSuccess SetreuidRuidSuccess
#define SetreuidCleanEuidSuccess SetreuidEuidSuccess
#define SetreuidCleanSetSvuid SetreuidSetSvuid

// APF_START(SetreuidCleanSetSvuid) {
//     return (// fc.params.at(1) != -1 && // REMOVED: Number (6) above
//             // Cleaned up standard specification (contains redundant
//             // "fc.params.at(1) != -1")
//             (fc.params.at(0) != -1 ||
//              (fc.params.at(1) != -1 &&
//               // Without (9) above:
//               P2U(fc.params.at(1)) != s1.ruid
//               // With (9) above:
//               // fc.params.at(1) != fc.params.at(0)
//                  )));
// } APF_FINISH

// Cleaning changes: relax constraints on setting svuid
APF_CLASSNAME(SetreuidCleanSvuidSuccess) {
    APF_HEADER(SetreuidCleanSvuidSuccess);
    APF_OP {
        return (
            s2.svuid == s1.svuid || // Darwin and OpenBSD: No change to svuid
            s2.svuid == s1.euid ||  // Darwin: svuid -> old_euid
            s2.svuid == s2.ruid ||  // OpenBSD: svuid -> new_ruid
            (!V(setSvuid) || s2.svuid == s2.euid) // All others: Follow standard
            );

        // REMOVED: Number (7) above
        // return (( V(setSvuid) && s2.svuid == s2.euid) || // REMOVED: Number (5) above
        //         (!V(setSvuid) && s2.svuid == s1.svuid));
    }
public:
    SetreuidCleanSvuidSuccess() :
        CONS_APF_CONST(SetreuidCleanSetSvuid, setSvuid) {}
private:
    APF_CONST(SetreuidCleanSetSvuid, setSvuid);
};

APF_CLASSNAME(SetreuidCleanSuccess) {
    APF_HEADER(SetreuidCleanSuccess);
    APF_OP {
        return (fc.rtn.value == 0 && V(ruidSuccess) &&
                V(euidSuccess) && V(svuidSuccess));
    }
public:
    SetreuidCleanSuccess() :
        CONS_APF_CONST(SetreuidCleanRuidSuccess, ruidSuccess),
        CONS_APF_CONST(SetreuidCleanEuidSuccess, euidSuccess),
        CONS_APF_CONST(SetreuidCleanSvuidSuccess, svuidSuccess) {}
private:
    APF_CONST(SetreuidCleanRuidSuccess, ruidSuccess);
    APF_CONST(SetreuidCleanEuidSuccess, euidSuccess);
    APF_CONST(SetreuidCleanSvuidSuccess, svuidSuccess);
};

// This is currently dead code, because...
// REMOVED: Number (8) above
APF_START(SetreuidCleanParamRuidIsValid) {
    return (fc.params.at(0) == -1 ||
            P2U(fc.params.at(0)) == s1.ruid ||
            P2U(fc.params.at(0)) == s1.euid);
} APF_FINISH

APF_START(SetreuidCleanParamEuidIsValid) {
    return (fc.params.at(1) == -1 ||
            P2U(fc.params.at(1)) == s1.ruid ||
            P2U(fc.params.at(1)) == s1.euid ||
            P2U(fc.params.at(1)) == s1.svuid); // Number (4a) above
} APF_FINISH

APF_CLASSNAME(SetreuidCleanImpliesAP) {
    APF_HEADER(SetreuidCleanImpliesAP);
    APF_OP {
        // HACK: Implementation-dependent ruidIsValid must be true or false,
        // but not both. In order for AP to be implied, it must NECESSARILY
        // BE IMPLIED. Therefore, try expression with ruidIsValid = true and
        // ruidIsValid = false; only if both are true, AP is implied.
        bool ruidIsValid;

        ruidIsValid = true;
        bool trueExpr = ((!V(success) && ruidIsValid && V(euidIsValid)) ||
                (!V(fail) && !V(einval) &&
                 (!ruidIsValid || !V(euidIsValid))));

        ruidIsValid = false;
        bool falseExpr = ((!V(success) && ruidIsValid && V(euidIsValid)) ||
                (!V(fail) && !V(einval) &&
                 (!ruidIsValid || !V(euidIsValid))));

        return (trueExpr && falseExpr);

        // REMOVED: Number (8) above
        // return ((!V(success) && V(ruidIsValid) && V(euidIsValid)) ||
        //         (!V(fail) && !V(einval) &&
        //          (!V(ruidIsValid) || !V(euidIsValid))));
    }
public:
    SetreuidCleanImpliesAP() :
        CONS_APF_CONST(SetreuidCleanSuccess, success),
        // CONS_APF_CONST(SetreuidCleanParamRuidIsValid, ruidIsValid),
        CONS_APF_CONST(SetreuidCleanParamEuidIsValid, euidIsValid),
        CONS_APF_CONST(Einval, einval),
        CONS_APF_CONST(Fail, fail) {}
private:
    APF_CONST(SetreuidCleanSuccess, success);
    // APF_CONST(SetreuidCleanParamRuidIsValid, ruidIsValid);
    APF_CONST(SetreuidCleanParamEuidIsValid, euidIsValid);
    APF_CONST(Einval, einval);
    APF_CONST(Fail, fail);
};

APF_CLASSNAME(SetreuidCleanImpliesNAP) {
    APF_HEADER(SetreuidCleanImpliesNAP);
    APF_OP {
        // HACK: Same hack as in SetreuidCleanImpliesAP
        bool ruidIsValid;

        ruidIsValid = true;

        bool trueExpr = ((!V(success) && !V(einval)) ||
                         (V(fail) && !V(einval) &&
                          (!ruidIsValid || !V(euidIsValid))));

        ruidIsValid = false;
        bool falseExpr = ((!V(success) && !V(einval)) ||
                          (V(fail) && !V(einval) &&
                           (!ruidIsValid || !V(euidIsValid))));

        return (trueExpr && falseExpr);

        // REMOVED: Number (8) above
        // return ((!V(success) && !V(einval)) ||
        //         (V(fail) && !V(einval) &&
        //          (!V(ruidIsValid) || !V(euidIsValid))));
    }
public:
    SetreuidCleanImpliesNAP() :
        CONS_APF_CONST(SetreuidCleanSuccess, success),
        // CONS_APF_CONST(SetreuidCleanParamRuidIsValid, ruidIsValid),
        CONS_APF_CONST(SetreuidCleanParamEuidIsValid, euidIsValid),
        CONS_APF_CONST(Einval, einval),
        CONS_APF_CONST(Fail, fail) {}
private:
    APF_CONST(SetreuidCleanSuccess, success);
    // APF_CONST(SetreuidCleanParamRuidIsValid, ruidIsValid);
    APF_CONST(SetreuidCleanParamEuidIsValid, euidIsValid);
    APF_CONST(Einval, einval);
    APF_CONST(Fail, fail);
};

// setuid(): !(AP && !AP) = true
APF_CLASSNAME(SetreuidCleanTautology) {
    APF_HEADER(SetreuidCleanTautology);
    APF_OP {
        // TODO: Just for testing
        bool const rtn = !(V(impliesAP) && V(impliesNAP));
        if (!rtn) {
            std::cerr << "SetreuidClean success: " << V(success) << std::endl;
            std::cerr << "SetreuidClean fail   : " << V(fail) << std::endl;
            std::cerr << "SetreuidClean einval : " << V(einval) << std::endl;
        }

        return !(V(impliesAP) && V(impliesNAP));
    }
public:
    SetreuidCleanTautology() :
        CONS_APF_CONST(SetreuidCleanImpliesAP, impliesAP),
        CONS_APF_CONST(SetreuidCleanImpliesNAP, impliesNAP), // {}

        // TODO: Temporary for testing
        CONS_APF_CONST(SetreuidCleanSuccess, success),
        CONS_APF_CONST(Einval, einval),
        CONS_APF_CONST(Fail, fail) {}
private:
    APF_CONST(SetreuidCleanImpliesAP, impliesAP);
    APF_CONST(SetreuidCleanImpliesNAP, impliesNAP);

    // TODO: Temporary for testing
    APF_CONST(SetreuidCleanSuccess, success);
    APF_CONST(Einval, einval);
    APF_CONST(Fail, fail);
};

// Setresuid

APF_START(SetresuidParamRuidIsValid) {
    UID const& p = P2U(fc.params.at(0));
    return (p == P2U(-1) || p == s1.ruid || p == s1.euid || p == s1.svuid);
} APF_FINISH

APF_START(SetresuidParamEuidIsValid) {
    UID const& p = P2U(fc.params.at(1));
    return (p == P2U(-1) || p == s1.ruid || p == s1.euid || p == s1.svuid);
} APF_FINISH

APF_START(SetresuidParamSvuidIsValid) {
    UID const& p = P2U(fc.params.at(2));
    return (p == P2U(-1) || p == s1.ruid || p == s1.euid || p == s1.svuid);
} APF_FINISH

APF_CLASSNAME(SetresuidParamsAreValid) {
    APF_HEADER(SetresuidParamsAreValid);
    APF_OP {
        return (V(ruidIsValid) && V(euidIsValid) && V(svuidIsValid));
    }
public:
    SetresuidParamsAreValid() :
        CONS_APF_CONST(SetresuidParamRuidIsValid, ruidIsValid),
        CONS_APF_CONST(SetresuidParamEuidIsValid, euidIsValid),
        CONS_APF_CONST(SetresuidParamSvuidIsValid, svuidIsValid) {}
private:
    APF_CONST(SetresuidParamRuidIsValid, ruidIsValid);
    APF_CONST(SetresuidParamEuidIsValid, euidIsValid);
    APF_CONST(SetresuidParamSvuidIsValid, svuidIsValid);
};

// NOTE: RuidSuccess and EuidSuccess are the same for setreuid() and setresuid()
#define SetresuidRuidSuccess SetreuidRuidSuccess
#define SetresuidEuidSuccess SetreuidEuidSuccess

APF_START(SetresuidSvuidSuccess) {
    return ((fc.params.at(2) == -1 && s2.svuid == s1.svuid) ||
            (fc.params.at(2) != -1 && s2.svuid == P2U(fc.params.at(2))));
} APF_FINISH

APF_CLASSNAME(SetresuidSuccess) {
    APF_HEADER(SetresuidSuccess);
    APF_OP {
        return (fc.rtn.value == 0 && V(ruidSuccess) &&
                V(euidSuccess) && V(svuidSuccess));
    }
public:
    SetresuidSuccess() :
        CONS_APF_CONST(SetresuidRuidSuccess, ruidSuccess),
        CONS_APF_CONST(SetresuidEuidSuccess, euidSuccess),
        CONS_APF_CONST(SetresuidSvuidSuccess, svuidSuccess) {}
private:
    APF_CONST(SetresuidRuidSuccess, ruidSuccess);
    APF_CONST(SetresuidEuidSuccess, euidSuccess);
    APF_CONST(SetresuidSvuidSuccess, svuidSuccess);
};

// setresuid(): !(AP && !AP) = true
APF_CLASSNAME(SetresuidTautology) {
    APF_HEADER(SetresuidTautology);
    APF_OP {
        return !((V(paramsAreValid) && !V(success) && V(einval)) ||
                 (!V(fail) && !V(einval) && !V(paramsAreValid) && !V(success)));
    }
public:
    SetresuidTautology() :
        CONS_APF_CONST(SetresuidSuccess, success),
        CONS_APF_CONST(SetresuidParamsAreValid, paramsAreValid),
        CONS_APF_CONST(Einval, einval),
        CONS_APF_CONST(Fail, fail) {}
private:
    APF_CONST(SetresuidSuccess, success);
    APF_CONST(SetresuidParamsAreValid, paramsAreValid);
    APF_CONST(Einval, einval);
    APF_CONST(Fail, fail);
};

// Setreuid for dropprivperm

APF_CLASSNAME(SetreuidForDropPrivPerm) {
    APF_HEADER(SetreuidForDropPrivPerm);
    APF_OP {
        if (fc.params.at(0) != fc.params.at(1)) {
            return true;
        }
        // else (params are the same):
        SetuidFunctionCall setresuidFn(fc);
        setresuidFn.function = Setresuid;
        setresuidFn.params.push_back(fc.params.at(0));
        return (V(setreuidSuccess) == setresuidSuccess(setresuidFn, s1, s2));
    }
public:
    SetreuidForDropPrivPerm() :
        CONS_APF_CONST(SetreuidSuccess, setreuidSuccess),
        CONS_APF_CONST(SetresuidSuccess, setresuidSuccess) {}
private:
    APF_CONST(SetreuidSuccess, setreuidSuccess);
    APF_CONST(SetresuidSuccess, setresuidSuccess);
};
