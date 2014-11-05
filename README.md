# Setuid Analysis, Verification, and Alternative Interface

This repository contains a snapshot of code based on *The UNIX Process
Identity Crisis: A Standards-Driven Approach to Setuid* by Mark S. Dittmer
and Mahesh V. Tripunitara. The first-order logic formalization of the POSIX
setuid standard can be found in *SetuidFormalization.pdf*.

*What the code can do*: The code in this repository can be used to construct
a setuid state graph on any system that implements POSIX setuid. The graph
can then be verified against
[POSIX.1-2008](http://pubs.opengroup.org/onlinepubs/9699919799/). The graph
can also be converted into C code that links against a simplified interface
that wraps setuid.

## Building and Running

### Prerequisites

The code requires libraries for Boost version 1.55.0. Newer version of Boost
may also work, but have not been tested.

### (G)make targets

The following GNU make targets are for building and running various parts of
the code:

- **exe**: Build executables needed for data collection, analysis, and code
  generation.

- **data**: Collect data from the running system. This will generate an
  archive containing a setuid state graph.

- **norm**: Normalize the user IDs in a data collection archive. The
  alternative interface expects data to be normalized.

- **code**: Generate C code (from a normalized data archive) that can be
  linked against the alternative setuid interface.

**Caveat 1**: Some make targets will prompt you for an administrator
password. That is because data collection must run as root (otherwise, not
all setuid states could be reached by the data collection program).

**Caveat 2**: Many systems (by default) impose strict limits on the number of
file descriptors or processes, either globally or per user. Data collection
may fail on such systems if these limits are not raised. If raising the
limits still doesn't work, try reducing `stateForkLimit` (in
`GraphExplorer.h`). Be aware that reducing this value will make data
collection take longer.

**Caveat 3**: Steps beyond data collection may not build and run on all
systems (even systems that were tested in the paper). The code and Makefile
are such that data collection is cross-platform, and data analysis and code
generation can be achieved on Linux.

**Caveat 4**: Make may report that it is ignoring circular dependencies on
generated archives or generated code. Such warnings can safely be ignored.

For more details, see `Makefile`.

## The Alternative Interface

*Background*: This code currently contains a snapshot of ongoing work. The
work contains a sound mechanism for manipulating user IDs, and a (presently)
unsound mechanism for manipulating both user and group IDs. The former is a
wrapper around the latter. The algorithms and verification results in the
paper only apply to the user-IDs-only interface. The unified users-and-groups
interface is not ready for general use.

The rest of this section addresses only the user-IDs-only interface.

### Changing User Identity

The setuid alternative presented in the paper (which wraps the setuid
function calls) contains two operations:

- `change_uid_temporarily(uid_t new_uid)`: Let `prev_euid` be the current
  effective user ID. This function sets the effective user ID to `new_uid`,
  and ensures that `prev_euid` gets copied to either the real or saved user
  ID.

- `change_uid_permanently(uid_t new_uid)`: This function ensures that
  `new_uid` is copied to the real, effective, and saved user IDs. When
  `new_uid` is unprivileged, this has the effect of making it impossible to
  switch to a different effective user ID for the remainder of the process
  lifetime.

To use the interface on a particular platform, you must generate
platform-specific code using the `code` make target, and include the `.c` and
`.h` files in `c/priv` and `c/priv2` in your project.

## Contact

Mark S. Dittmer: mdittmer@uwaterloo.ca
