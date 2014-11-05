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


#include "Platform.h"

#if IS_SOLARIS

#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <procfs.h>

// Copied from Tsafrir, et al's implementation at:
// https://code.google.com/p/change-process-identity
#  define DO_SYS(call) if( (call) == -1 ) return -1
#  define DO_CHK(expr) if( ! (expr)     ) return -1

static int get_prcred(prcred_t *p)
{
    int fd;

    DO_SYS( fd = open("/proc/self/cred", O_RDONLY) );
    DO_CHK( read(fd, p, sizeof(*p)) == sizeof(*p)  );
    DO_SYS( close(fd)                              );
    return 0;
}

int getresuid(uid_t *ruid, uid_t *euid, uid_t *suid)
{
    if (!ruid || !euid || !suid) {
        errno = EINVAL;
        return -1;
    }

    int rtn;
    prcred_t prc;
    if ((rtn = get_prcred(&prc)) != 0) {
        return rtn;
    }
    *ruid = prc.pr_ruid;
    *euid = prc.pr_euid;
    *suid = prc.pr_suid;

    return 0;
}

int setresuid(uid_t ruid, uid_t euid, uid_t suid)
{
    prcred_t prc; // it's pRcred_t, not to be confused with our pcred_t
    DO_SYS( get_prcred(&prc) );
    if( geteuid() == 0 ) {

	// helper vars
	int      fd;
	long     cmd = PCSCRED;
	iovec_t  iov[2] = { {(caddr_t)&cmd, sizeof(cmd)},
			    {(caddr_t)&prc, sizeof(prc)} };

	// prepare process credentials
	prc.pr_ruid = ruid != (uid_t)(-1) ? ruid : prc.pr_ruid;
	prc.pr_euid = euid != (uid_t)(-1) ? euid : prc.pr_euid;
	prc.pr_suid = suid != (uid_t)(-1) ? suid : prc.pr_suid;
	prc.pr_ngroups = 0;

	// set them through /proc
	DO_SYS( (fd = open("/proc/self/ctl", O_WRONLY)) == -1 );
	DO_CHK( writev(fd,iov,2) == (sizeof(cmd) + sizeof(prc)) );
    }
    else {
	// this by no means works for the general case; see documentation
	// of setreuid() above. but it does work for the typical non-root
	// setuid case where (1) the real and saved IDs are different and
	// remain constant, and (3) only the effective ID is juggled.
	DO_SYS( setreuid(
                    ruid != (uid_t)(-1) ? ruid : prc.pr_ruid,
                    suid != (uid_t)(-1) ? suid : prc.pr_suid) ); // copies suid to saved (as ruid <> -1)
	DO_SYS( seteuid(
                    euid != (uid_t)(-1) ? euid : prc.pr_euid) ); // doesn't affect ruid or suid
    }

    return 0;
}

#endif

// TODO: It would be nice to figure out a setresuid() equivalent for Darwin
