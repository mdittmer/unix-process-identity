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


#include "GraphName.h"

#include "Graph.h"

#include <iostream>
#include <utility>
#include <vector>

GraphName::GraphName(
    std::string const& basename,
    UIDSet const& uidSet,
    ParamSet const& paramSet) {
    std::vector<int> uids, params;
    for (UIDSet::const_iterator it = uidSet.begin(), ie = uidSet.end();
         it != ie; ++it) {
        uids.push_back(static_cast<int>(*it));
    }
    std::sort(uids.begin(), uids.end());
    for (ParamSet::const_iterator it = paramSet.begin(), ie = paramSet.end();
         it != ie; ++it) {
        params.push_back(*it);
    }
    std::sort(params.begin(), params.end());

    std::stringstream ss;
    ss << basename << "__u_";
    for (std::vector<int>::iterator it = uids.begin(), ie = uids.end();
         it != ie; ++it) {
        ss << *it;
        if (it +1 != uids.end()) {
            ss << "_";
        }
    }
    ss << "__p_";
    for (std::vector<int>::iterator it = params.begin(), ie = params.end();
         it != ie; ++it) {
        ss << *it;
        if (it +1 != params.end()) {
            ss << "_";
        }
    }

    name = ss.str();
}

std::ostream& operator<<(std::ostream& os, GraphName const& name) {
    os << name.getName();
    return os;
}
