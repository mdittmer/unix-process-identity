//##############################################################################
// priv.c
//     Securely change process identity
//     Written by Dan Tsafrir and David Wagner
//     Modified by Mark Dittmer
// See also
//     http://code.google.com/p/change-process-identity
//     http://www.usenix.org/publications/login/2008-06/pdfs/tsafrir.pdf
//##############################################################################


//##############################################################################
// Include
//##############################################################################
#ifdef __linux__
#  define _GNU_SOURCE		/* {set,get}resuid */
#  define __USE_GNU		/* getline */
#endif
#include <stdio.h>		/* driver */
#include <stdbool.h>		/* bool */
#include <stdlib.h>		/* exit, qsort, malloc, abort */
#include <string.h>		/* malloc */
#include <assert.h>		/* assert */
#include <sys/types.h>		/* open */
#include <unistd.h>		/* setgroups, getgroups, sysconf */
#include <grp.h>		/* setgroups */
#include <sys/stat.h>		/* open */
#include <fcntl.h>		/* open */
#include <errno.h>              /* errno */
#if defined(__sun__) || defined (__sun)
#  include <sys/uio.h>
#  include <procfs.h>
#endif
#if defined(_AIX)
#  include <sys/id.h>
#endif
#if defined(__APPLE__) && defined(__MACH__)
#  include <libproc.h>
#  include <sys/proc_info.h>
#endif
#include "priv.h"


//##############################################################################
// Macros
//##############################################################################
#ifdef LIVING_ON_THE_EDGE /* errors can be reported by return values, but... */
#  define DO_SYS(call) if( (call) == -1 ) return -1
#  define DO_CHK(expr) if( ! (expr)     ) return -1
#elif 1 /* should use these, as we don't do any cleanups/rollbacks */
#  define DO_SYS(call) if( (call) == -1 ) abort()
#  define DO_CHK(expr) if( ! (expr)     ) abort()
#else   /* for debugging, however, this is best */
#  define DO_SYS(call) assert( (call) != -1 )
#  define DO_CHK(expr) assert( expr )
#endif


//##############################################################################
// Internal data structures
//##############################################################################
// Modified by mdittmer: made non-static (placed in priv.h)
// typedef struct user_ids  { uid_t r, e, s; } uids_t; // real, effective, saved
// typedef struct group_ids { gid_t r, e, s; } gids_t;
// typedef struct process_credentials {
//     uids_t uids;
//     gids_t gids;
//     sups_t sups;
// } pcred_t;


//##############################################################################
// Internal prototypes
//##############################################################################
// Modified by mdittmer: made non-static (placed in priv.h)
// static bool ucred_is_sane(const ucred_t *uc);
// static int  get_pcred    (pcred_t *pc);
// static bool eql_sups     (const sups_t *cursups, const sups_t *targetsups);
// static int  get_sups     (sups_t *s);
static int  get_saved_ids(uid_t *suid , gid_t *sgid);
static int  set_uids     (uid_t  ruid , uid_t  euid, uid_t suid);
// Added by mdittmer: Support set uids only
#ifndef SET_UIDS_ONLY
static int  set_gids     (gid_t  rgid , gid_t  egid, gid_t sgid);
// Modified by mdittmer: made non-static (placed in priv.h)
// static int  set_sups     (const sups_t *targetsups);
#endif
#ifdef __linux__
static int  get_fs_ids   (uid_t *fsuid, gid_t *fsgid);
#endif

// debug
#ifdef DEBUG_PRIV
// Modified by mdittmer: made non-static (placed in priv.h)
// static void print_sups (const char *title, const sups_t  *s);
// static void print_ucred(const char *title, const ucred_t *u);
// static void print_pcred(const char *title, const pcred_t *p);
#endif


//##############################################################################
// Higher level algorithm
//##############################################################################

//------------------------------------------------------------------------------
// Permanently change identity of a privileged process (root, or setuid) to
// that a of the given non-privileged user.
//
// o We verify, at the end of the algorithm, that the identity was
//   indeed changed, because manuals sometimes lie, operating systems
//   sometimes have bugs, setuid functions might have silent failures
//   [dean04], there exist strange interactions between setgroups and
//   set*gid system-calls [FreeBSD], the semantics of set*id calls might
//   change between kernel versions (especially since what we do isn't
//   necessarily POSIX), our set_uid and set_gid are only best effort
//   (e.g., see solaris_set_{uids,gids}). In general, verifying that the
//   identity change succeeded is important, because a wrong identity
//   might have severe consequences.
//
// o The order of operations is important (uid settings after gid settings
//   so it's possible to change groups; primary after supplementary groups
//   settings due to noncompliant FreeBSD behavior, see below.)
//
// o Upon failure, in addition to being in an inconsistent state in terms of
//   identity, we're also leaking memory => We need the abort() version of the
//   DO_* macros.
//------------------------------------------------------------------------------
int drop_privileges_permanently(const ucred_t *uc/*target identity*/)
{
    uid_t   u = uc->uid;
// Added by mdittmer: Support set uids only
#if !defined(SET_UIDS_ONLY) || defined(__linux__)
    gid_t   g = uc->gid;
#endif
    pcred_t pc;

    DO_CHK( ucred_is_sane(uc) );

    // change identity (order is important)
// Added by mdittmer: Support set uids only
#ifndef SET_UIDS_ONLY
    DO_SYS( set_sups( &uc->sups )              );
    DO_SYS( set_gids( g/*real*/, g/*effective*/, g/*saved*/ ) ); // after sups!
#endif
    DO_SYS( set_uids( u/*real*/, u/*effective*/, u/*saved*/ ) ); // last!

    // verify that identity was changed as expected
    DO_SYS( get_pcred( &pc )                                       );
// Added by mdittmer: Support set uids only
#ifndef SET_UIDS_ONLY
    DO_CHK( eql_sups ( &pc.sups , &uc->sups )                      );
    DO_CHK( g == pc.gids.r  &&  g == pc.gids.e  &&  g == pc.gids.s );
#endif
    DO_CHK( u == pc.uids.r  &&  u == pc.uids.e  &&  u == pc.uids.s );
    free( pc.sups.list );

#if defined(__linux__)
    DO_SYS( get_fs_ids(&u,&g) );
    DO_CHK( u == uc->uid      );
// Added by mdittmer: Support set uids only
#ifndef SET_UIDS_ONLY
    DO_CHK( g == uc->gid      );
#endif
#endif

    return 0;
}

//------------------------------------------------------------------------------
// Switch to the given user, but maintain the ability to regain the
// current effective identity. Specifically,
//
// o We're currently "privileged", and uc holds the EFFECTIVE
//   "unprivileged" identity as we would like it to be after this
//   function is invoked. In the furture, the calling process would like
//   to be able to regain the current effective identity.
//
// o This would be achievable in a straightforward manner if the
//   current effective identity is also either the real or the saved
//   identity.  This is true becase the POSIX definition of seteuid()
//   says:
//
//    ``If uid is equal to the real user ID or the saved set-user-ID, or if
//      the process has appropriate privileges, seteuid() shall set the
//      effective user ID of the calling process to uid; the real user ID
//      and saved set-user-ID shall remain unchanged.''
//
//   [ www.opengroup.org/onlinepubs/009695399/functions/seteuid.html ]
//
// o In ocntrast to [chen02], only if we have no other choice, will we
//   attempt to modify the saved ID to hold the current effective ID. We
//   try our best to avoid setting the saved IDs, because some OSes do
//   NOT allow non-root set*id processes to change the saved ID [AIX];
//   (BTW, note that this means that in AIX it's impossible for a
//   non-root set*id process to drop privileges permanently.)
//
// o We set user last (to be able to manipulate groups), and primary
//   after supplementary groups (to accommodate FreeBSD's nonstandard
//   behavior).
//------------------------------------------------------------------------------
int drop_privileges_temporarily(const ucred_t *uc)
{
    uid_t newEuid=uc->uid, newSuid;
// Added by mdittmer: Support set uids only
#ifndef SET_UIDS_ONLY
    gid_t newEgid=uc->gid, newSgid;
#endif
    pcred_t old, new;

    DO_CHK( ucred_is_sane(uc) );

    // examine current settings: we'll change the saved IDs only if have to
    DO_SYS( get_pcred(&old) );

    newSuid = ( old.uids.e == old.uids.r  ||  old.uids.e == old.uids.s )
	? old.uids.s : newEuid;

// Added by mdittmer: Support set uids only
#ifndef SET_UIDS_ONLY
    newSgid = ( old.gids.e == old.gids.r  ||  old.gids.e == old.gids.s )
	? old.gids.s : newEgid;
#endif

    // change settings
// Added by mdittmer: Support set uids only
#ifndef SET_UIDS_ONLY
    DO_SYS( set_sups( &uc->sups )                    );
    DO_SYS( set_gids( old.gids.r, newEgid, newSgid ) ); // after set_sups!
#endif
    DO_SYS( set_uids( old.uids.r, newEuid, newSuid ) ); // after groups!

    // verify that settings changed as expected
    DO_SYS( get_pcred( &new )                 );
// Added by mdittmer: Support set uids only
#ifndef SET_UIDS_ONLY
    DO_CHK( eql_sups ( &new.sups, &uc->sups ) );
    DO_CHK( old.gids.r == new.gids.r          );
    DO_CHK( newEgid == new.gids.e             );
    DO_CHK( newSgid == new.gids.s             );
#endif
    DO_CHK( old.uids.r == new.uids.r          );
    DO_CHK(    newEuid == new.uids.e          );
    DO_CHK(    newSuid == new.uids.s          );

    free( old.sups.list );
    free( new.sups.list );

    return 0;
}

//------------------------------------------------------------------------------
// The reverse operation of drop_privileges_temporarily().
//
// o Note that a full target identity 'uc' is required, as we don't
//   know whether to restore the effective IDs from the real ID or the
//   saved ID, and we also don't know what were the supplementary groups
//   prior to dropping the privileges.
//
// o Operations here are done in reverse order relative to
//   drop_privileges_temporarily: We set user first (to be able to
//   manipulate groups), and primary group after supplementary groups
//   (to accommodate FreeBSD's nonstandard behavior).
//
// o Once again, we are interested only in the effective IDs; the real and
//   saved IDs are irrelevant and will remain unchanged.
//------------------------------------------------------------------------------
int restore_privileges(const ucred_t *uc)
{
    uid_t u = uc->uid;
// Added by mdittmer: Support set uids only
#ifndef SET_UIDS_ONLY
    gid_t g = uc->gid;
#endif
    pcred_t pc;

    DO_CHK( ucred_is_sane(uc) );

    // restore setting
    if( u != geteuid() ) DO_SYS( seteuid(u) ); // first!
// Added by mdittmer: Support set uids only
#ifndef SET_UIDS_ONLY
    DO_SYS( set_sups(&uc->sups) );
    if( g != getegid() ) DO_SYS( setegid(g) ); // after set_sups!
#endif

    // verify
    DO_SYS( get_pcred(&pc)  );
    DO_CHK( u == pc.uids.e  );
// Added by mdittmer: Support set uids only
#ifndef SET_UIDS_ONLY
    DO_CHK( g == pc.gids.e  );
    DO_CHK( eql_sups (&pc.sups, &uc->sups) );
#endif
    free(pc.sups.list);

    return 0;
}

//------------------------------------------------------------------------------
// Return true if the given credentials object is sane.
//------------------------------------------------------------------------------
bool ucred_is_sane(const ucred_t *uc)
{
    long   nm  = sysconf(_SC_NGROUPS_MAX);
    long   i,n = uc->sups.size;
    gid_t *arr = uc->sups.list;

    // make sure supplementary groups are sorted ascending with no duplicates
    for(i=1; i<n; i++)
	if( arr[i-1] >= arr[i] )
	    return false;

    // the uid/gid shouldn't be -1, because it has special meaning for
    // set*id function ("no change"), whereas our interface mandates
    // the caller to explicitly specify the target IDs. also, the size
    // of the supplementary gropus array should be reasonable.
    // Added by mdittmer: Check return value and set errno
    bool rtn = (nm >= 0) && (nm >= n) && (n >= 0) &&
        (uc->uid != (uid_t)-1) &&
        (uc->gid != (gid_t)-1);
    if (!rtn) {
        errno = EINVAL;
    }
    return rtn;
}


//##############################################################################
// Changing supplementary groups
//##############################################################################

//------------------------------------------------------------------------------
// Only root is allowed to invoke setgroups(). Note, however, that for
// the canonical case of a non-root set[ug]id program, there's no need
// to change the supplementary list when dropping privileges: it
// already corresponds to the real (rather than effective) user.
//------------------------------------------------------------------------------
int set_sups(const sups_t *orig_targetsups)
{
    sups_t targetsups = *orig_targetsups;

#ifdef __FreeBSD__	   /* FreeBSD uses entry [0] of sup arr as egid... */
    gid_t arr[ targetsups.size + 1 ];
    memcpy(arr+1, targetsups.list, targetsups.size * sizeof(gid_t) );
    targetsups.size    = targetsups.size + 1;
    targetsups.list    = arr;
    targetsups.list[0] = getegid();
#endif

    if( geteuid() == 0 ) { // allowed to setgroups, let's not take any chances
	DO_SYS( setgroups(targetsups.size, targetsups.list) );
    }
    else {
	sups_t cursups;
	DO_SYS( get_sups(&cursups) );
	if( ! eql_sups(&cursups, &targetsups) )
	    // this will probably fail... :(
	    DO_SYS( setgroups(targetsups.size, targetsups.list) );
	free( cursups.list );
    }

    return 0;
}

//------------------------------------------------------------------------------
// Comparing the current supplementary list (that we'd possibly want to change
// in the higher level algorithm above) to the target supplementary list.
//
// o This function is needed for two reasons
//   1) 'cursups' has just been retrived with getgroups() and, given the target
//      supplementary list 'targetsups', we're about to determine if a call to
//      setgroups is needed.
//   2) We want to verify that a change in the supplementary groups has
//      happened as expected.
//
// o Doing this is a bit tricky due to the semantics of getgroups:
//
//   ``It is implementation-defined whether getgroups() also returns the
//     effective group ID in the grouplist array.''
//
//   [ www.opengroup.org/onlinepubs/000095399/functions/getgroups.html ]
//
// o In other words, if the returned list does contain egid, the process has
//   no straightforward way to determine whether the egid is or isn't part
//   of its supplementary list.
//
// o Consequently, this function ignores the situation in which egid is found
//   in cursups but not in targetsups: if this is the only difference between
//   the two sets and the function will return true, despite the difference.
//
// o Note that this is not completely safe!
//
// o We nevertheless contend that this is still a reasonable policy because
//   1) It the interface is used by root then set_sups() doesn't use this
//      function, but rather simply setgroups() directly.
//   2) If the invoker is non-root then it means he is not allowed to use
//      setgroups, which means the groups remain as they were when the ruid
//      executed the program, which is the correct thing to do when dropping
//      privileges: a non-root setuid program in any case can't switch to
//      any identity other than the real one.
//------------------------------------------------------------------------------
bool eql_sups(const sups_t *cursups, const sups_t *targetsups)
{
    int   i, j, n = targetsups->size;
    int   diff    = cursups->size - targetsups->size;
    gid_t egid    = getegid();

    if( diff > 1 || diff < 0 ) return false;

    for(i=0, j=0; i < n; i++, j++)
	if( cursups->list[j] != targetsups->list[i] ) {
	    if( cursups->list[j] == egid ) i--; // skips j
	    else                           return false;
	}

    // if reached here, we're sure i==targetsups->size. now, either we
    // j==cursups->size (skipped egid, or it wasn't there), or we didn't
    // get to the egid yet because it's the last entry in cursups
    return j == cursups->size  ||
	(j+1 == cursups->size  &&  cursups->list[j] == egid);
}

//##############################################################################
// Get current credentials of a process
//##############################################################################

//------------------------------------------------------------------------------
// Get process credentials: most is straightforward, with the exception
// of saved IDs and Linux's fs IDs, which are OS specific
//------------------------------------------------------------------------------
// Modified by mdittmer: made non-static
int get_pcred(pcred_t *pc)
{
    uid_t suid;
    gid_t sgid;

    DO_SYS( get_saved_ids(&suid, &sgid) );
    DO_SYS( get_sups(&pc->sups)         );

    pc->uids.r = getuid();
    pc->uids.e = geteuid();
    pc->uids.s = suid;

    pc->gids.r = getgid();
    pc->gids.e = getegid();
    pc->gids.s = sgid;

    return 0;
}

//------------------------------------------------------------------------------
// Comparison function for qsort: compares two gids.
//------------------------------------------------------------------------------
static int cmp_gid(const void *x, const void *y)
{
    return *((gid_t*)x) - *((gid_t*)y);
}

//------------------------------------------------------------------------------
// Get the supplementary group, malloc()ing space as appropriate, and
// normalizing the array (remove duplicates, sort ascending) such that
// a comparison (eql_sups) would be meaningful.
//
// [ Duplicates: POSIX makes no promises in this respect. ]
//------------------------------------------------------------------------------
// Modified by mdittmer: exported from internal protocol
int get_sups(sups_t *s)
{
    int i, j, size = getgroups(0,NULL);

    DO_CHK( size >= 0 );

    if( size > 0 ) {

	gid_t arr[size];

	// fill, sort, allocate
	DO_CHK( size >= 0            );
	DO_SYS( getgroups(size, arr) );
	qsort(arr, size, sizeof(gid_t), cmp_gid);
	s->list = malloc( sizeof(gid_t)*size );

	// uniqify
	for(i=0, j=0; i < size; i++)
	    if( i==0 || arr[i] > arr[i-1] )
		s->list[j++] = arr[i];

	s->size = j;
    }
    else {
	s->size = 0;
	s->list = NULL;
    }

    return 0;
}

//------------------------------------------------------------------------------
// Read the process credentials from the /proc on a solaris system.
// See man proc(4) [ http://docs.sun.com/app/docs/doc/816-5174/6mbb98ui0 ]
//------------------------------------------------------------------------------
#if defined(__sun__) || defined (__sun)
static int solaris_get_prcred(prcred_t *p)
{
    int fd;

    DO_SYS( fd = open("/proc/self/cred", O_RDONLY) );
    DO_CHK( read(fd, p, sizeof(*p)) == sizeof(*p)  );
    DO_SYS( close(fd)                              );
    return 0;
}
#endif /*solaris*/

//------------------------------------------------------------------------------
// Get saved IDs.
//------------------------------------------------------------------------------
static int get_saved_ids(uid_t *suid, gid_t *sgid)
{
#if defined(__linux__)   || defined(__HPUX__)    || \
    defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
    uid_t ruid, euid;
    gid_t rgid, egid;
    DO_SYS( getresuid(&ruid, &euid, suid) );
    DO_SYS( getresgid(&rgid, &egid, sgid) );

#elif defined(_AIX)
    DO_SYS( *suid = getuidx(ID_SAVED) );
    DO_SYS( *sgid = getgidx(ID_SAVED) );

#elif defined(__sun__) || defined(__sun)
    prcred_t prc;
    DO_SYS( solaris_get_prcred(&prc) );
    *suid = prc.pr_suid;
    *sgid = prc.pr_sgid;

#elif defined(__APPLE__) && defined(__MACH__)
    pid_t pid = getpid();
    struct proc_bsdshortinfo info;
    int syscallRtn = proc_pidinfo(
        pid,
        PROC_PIDT_SHORTBSDINFO,
        0,
        &info,
        sizeof(info));
    DO_CHK( syscallRtn == sizeof(info) );
    *suid = info.pbsi_svuid;
    *sgid = info.pbsi_svgid;

#else
#  error no implementation (notably __NetBSD__, __CYGWIN__)

#endif
    return 0;
}

//------------------------------------------------------------------------------
// Linux filesystem IDs.
//------------------------------------------------------------------------------
#ifdef __linux__
static int get_fs_ids(uid_t *fsuid, gid_t *fsgid)
{
    unsigned r, e, s, fs;
    // Added by mdittmer: bug fix: got_u and got_g need initialization
    bool     got_u = false, got_g = false;
    char    *line = NULL;
    size_t   len  = 0;
    FILE    *fp;

    *fsuid = * fsgid = -1;
    DO_CHK( (fp = fopen("/proc/self/status", "r")) != NULL );

    while( getline(&line, &len, fp) != -1 ) {
	if( ! got_u && sscanf(line,"Uid: %u %u %u %u", &r, &e, &s, &fs)==4 ) {
	    got_u  = true;
	    *fsuid = fs;
	}
	if( ! got_g && sscanf(line,"Gid: %u %u %u %u", &r, &e, &s, &fs)==4 ) {
	    got_g  = true;
	    *fsgid = fs;
	}
	if( got_u && got_g )
	    break;
    }

    if( line )
	free(line);

    return (got_u && got_g) ? 0 : -1;
}
#endif /*linux*/


//##############################################################################
// Setting uids
//##############################################################################
#ifdef _AIX
static int aix_set_uids(uid_t ruid, uid_t euid, uid_t suid);
#endif

#if defined(__sun__) || defined(__sun)
static int solaris_set_uids(uid_t ruid, uid_t euid, uid_t suid);
#endif

#if defined(__APPLE__) && defined(__MACH__)
static int darwin_set_uids(uid_t ruid, uid_t euid, uid_t suid);
#endif

//------------------------------------------------------------------------------
// Set the given values to be the real, effective, and saved IDs of the
// current process. OSes that have setresgid() make it very simple;
// with the others, we have to get our hands dirty (and contrary to
// getting IDs, here we can only try our best, depending on what the
// OS allows).
//------------------------------------------------------------------------------
static int set_uids(uid_t ruid, uid_t euid, uid_t suid)
{
#if defined(__linux__) || defined(__HPUX__)    || \
 defined(__FreeBSD__)  || defined(__OpenBSD__) || defined(__DragonFly__)
    DO_SYS( setresuid(ruid, euid, suid) );

#elif defined(_AIX)
    DO_SYS( aix_set_uids(ruid, euid, suid) );

#elif defined(__sun__) || defined(__sun)
    DO_SYS( solaris_set_uids(ruid, euid, suid) );

#elif defined(__APPLE__) && defined(__MACH__)
    DO_SYS( darwin_set_uids(ruid, euid, suid) );

#else
#   error need to implement (notably __NetBSD__, __CYGWIN__)

#endif
    return 0;
}

//------------------------------------------------------------------------------
// In AIX set*id functions are more restrictive in comparison to what
// setresuid allows: euid is _always_ either ruid or suid; ruid/suid
// can't be modified by a non-root process; and even root has severe
// restrictions. Specifically,
//
// o AIX's most expressive set*uid system call is setuidx(), which has
//   two arguments. The first must be one of these three values:
//   (1) ID_EFFECTIVE
//   (2) ID_EFFECTIVE|ID_REAL
//   (3) ID_EFFECTIVE|ID_REAL|ID_SAVED
//   The second argument is the value to assign to the IDs specified by
//   the first argument.
//
// o A non-root setuid program is allowed to use only the first value
//   (which means it can only change its effective identity and hence
//   can't drop privileges permanently).
//------------------------------------------------------------------------------
#ifdef _AIX
static int aix_set_uids(uid_t ruid, uid_t euid, uid_t suid)
{
    gid_t cursgid;
    uid_t curruid=getuid(), cureuid=geteuid(), cursuid;
    DO_SYS( get_saved_ids(&cursuid, &cursgid) );

    if( euid == cureuid && ruid == curruid && suid == cursuid )
	return 0;
    else if( euid != cureuid && ruid == curruid && suid == cursuid )
	DO_SYS( setuidx(ID_EFFECTIVE, euid) );
    else if( euid == ruid && ruid != curruid && suid == cursuid )
	DO_SYS( setuidx(ID_EFFECTIVE|ID_REAL, euid) );
    else if( euid == ruid && ruid == suid )
	DO_SYS( setuidx(ID_EFFECTIVE|ID_REAL|ID_SAVED, euid) );
    else
	DO_CHK( false ); // AIX doesn't allow anything else :(

    return 0;
}
#endif /*_AIX*/

//------------------------------------------------------------------------------
// Set uids in [open]solaris.
//
// o Root can set the IDs in a straightforward manner, using the proc interface,
//   see: man proc(4) [ docs.sun.com/app/docs/doc/816-5174/6mbb98ui0 ]
//   Unlike in other OSes, the proc interface is well documented and stable.
//
// o For a non-root user, we have to resort to using setreuid() (the function
//   ceases to be a general set_uids and becomes best effort). The relevant
//   documentation of the solaris version of setreuid() is:
//
//   - "if the real user ID is being changed (that is, if ruid is
//     not -1), or the effective user ID is being changed to a value
//     not equal to the real user ID, the saved set-user ID is set
//     equal to the new effective user ID."
//
//   - error will occur if a "change was specified other than
//     changing the real user ID to the effective user ID, or
//     changing the effective user ID to the real user ID or the
//     saved set-user ID."
//
//     [ docs.sun.com/app/docs/doc/819-2241/6n4huc7o3?l=en&a=view&q=setreuid ]
//
//------------------------------------------------------------------------------
#if defined(__sun__) || defined(__sun)
static int solaris_set_uids(uid_t ruid, uid_t euid, uid_t suid)
{
    if( geteuid() == 0 ) {

	// helper vars
	int      fd;
	prcred_t prc; // it's pRcred_t, not to be confused with our pcred_t
	long     cmd = PCSCRED;
	iovec_t  iov[2] = { {(caddr_t)&cmd, sizeof(cmd)},
			    {(caddr_t)&prc, sizeof(prc)} };

	// prepare process credentials
	DO_SYS( solaris_get_prcred(&prc) );
	prc.pr_ruid = ruid;
	prc.pr_euid = euid;
	prc.pr_suid = suid;
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
	DO_SYS( setreuid(ruid, suid) ); // copies suid to saved (as ruid <> -1)
	DO_SYS( seteuid(euid)        ); // doesn't affect ruid or suid
    }

    return 0;
}
#endif /*solaris*/


//------------------------------------------------------------------------------
// Set uids in Darwin (Mac OS X).
//------------------------------------------------------------------------------
#if defined(__APPLE__) && defined(__MACH__)
static int darwin_set_uids(uid_t ruid, uid_t euid, uid_t suid)
{
    // TODO: There doesn't appear to be a way to effectively setresuid on Darwin
    // when euid = 0
    DO_CHK( geteuid() != 0 );

    // Same as non-root Solaris implementation
    // this by no means works for the general case; see documentation
    // of setreuid() above. but it does work for the typical non-root
    // setuid case where (1) the real and saved IDs are different and
    // remain constant, and (3) only the effective ID is juggled.
    DO_SYS( setreuid(ruid, suid) ); // copies suid to saved (as ruid <> -1)
    DO_SYS( seteuid(euid)        ); // doesn't affect ruid or suid

    return 0;
}
#endif /*darwin*/


//##############################################################################
// Setting gids
//##############################################################################

//------------------------------------------------------------------------------
// Set gids for AIX.
//------------------------------------------------------------------------------
#ifdef _AIX
static int aix_set_gids(gid_t rgid, gid_t egid, gid_t sgid)
{
    uid_t cursuid;
    gid_t currgid=getgid(), curegid=getegid(), cursgid;
    DO_SYS( get_saved_ids(&cursuid, &cursgid) );

    if( egid == curegid && rgid == currgid && sgid == cursgid )
	return 0;
    else if( egid != curegid && rgid == currgid && sgid == cursgid )
	DO_SYS( setgidx(ID_EFFECTIVE, egid) );
    else if( egid == rgid && rgid != currgid && sgid == cursgid )
	DO_SYS( setgidx(ID_EFFECTIVE|ID_REAL, egid) );
    else if( egid == rgid && rgid == sgid )
	DO_SYS( setgidx(ID_EFFECTIVE|ID_REAL|ID_SAVED, egid) );
    else
	DO_CHK( false ); // AIX doesn't allow anything else :(

    return 0;
}
#endif /*_AIX*/

//------------------------------------------------------------------------------
// Setgid for solaris.
//------------------------------------------------------------------------------
#if defined(__sun__) || defined(__sun)
static int solaris_set_gids(gid_t rgid, gid_t egid, gid_t sgid)
{
    if( geteuid() == 0 ) {

	// helper vars
	int      fd;
	prcred_t prc;
	long     cmd = PCSCRED;
	iovec_t  iov[2] = { {(caddr_t)&cmd, sizeof(cmd)},
			    {(caddr_t)&prc, sizeof(prc)} };

	// prepare process credentials
	DO_SYS( solaris_get_prcred(&prc) );
	prc.pr_rgid = rgid;
	prc.pr_egid = egid;
	prc.pr_sgid = sgid;
	prc.pr_ngroups = 0;

	// set them throug /proc
	DO_SYS( (fd = open("/proc/self/ctl", O_WRONLY)) == -1   );
	DO_CHK( writev(fd,iov,2) != (sizeof(cmd) + sizeof(cmd)) );
    }
    else { // works for the typical case
	DO_SYS( setregid(rgid, sgid) );
	DO_SYS( setegid(egid)        );
    }

    return 0;
}
#endif /*solaris*/

//------------------------------------------------------------------------------
// Set gids.
//------------------------------------------------------------------------------
// Added by mdittmer: Support set uids only
#ifndef SET_UIDS_ONLY
static int set_gids(gid_t rgid, gid_t egid, gid_t sgid)
{
#if defined(__linux__) || defined(__HPUX__)    || \
 defined(__FreeBSD__)  || defined(__OpenBSD__) || defined(__DragonFly__)
    DO_SYS( setresgid(rgid, egid, sgid) );

#elif defined(_AIX)
    DO_SYS( aix_set_gids(rgid, egid, sgid) );

#elif defined(__sun__) || defined(__sun)
    DO_SYS( solaris_set_gids(rgid, egid, sgid) );

#else
#   error need to implement (notably __NetBSD__, __APPLE__, __CYGWIN__)

#endif
    return 0;
}
#endif


//##############################################################################
// Driver & debug
//##############################################################################

//------------------------------------------------------------------------------
// Print supplementary group list.
//------------------------------------------------------------------------------
void print_sups(const char *title, const sups_t *s)
{
    if( title ) printf("%-12s   ", title);

    if( s->size ) {
	int i;
	for(i=0; i < s->size; i++)
	    printf("%s%u", (i ? " " : "["), (unsigned)s->list[i]);
	printf("]");
    }
    else {
	printf("[empty]");
    }

    if( title ) printf("\n");
}

//------------------------------------------------------------------------------
// Print process credentials.
//------------------------------------------------------------------------------
void print_pcred(const char *title, const pcred_t *p)
{
    printf("%-12s   ruid=%-6d   rgid=%d\n",title,(int)p->uids.r,(int)p->gids.r);
    printf("%-12s   euid=%-6d   egid=%d\n",title,(int)p->uids.e,(int)p->gids.e);
    printf("%-12s   suid=%-6d   sgid=%d\n",title,(int)p->uids.s,(int)p->gids.s);
    printf("%-12s   supp: ", title);
    print_sups(NULL, &p->sups);
    printf("\n");
    printf("-------------------------------------------------------\n");

    fflush(stdout);
}

//------------------------------------------------------------------------------
// Print user credentials.
//------------------------------------------------------------------------------
void print_ucred(const char *title, const ucred_t *u)
{
    printf("%-12s   uid=%-6d   gid=%-6d ", title, (int)u->uid, (int)u->gid);
    print_sups(NULL, &u->sups);
    printf("\n");

    fflush(stdout);
}

//------------------------------------------------------------------------------
// Driver.
//------------------------------------------------------------------------------
#ifdef TEST_PRIV
int main(int argc, char *argv[])
{
    unsigned uid, gid;
    ucred_t targetu, origu;
    pcred_t p1, p2, p3;
    const int NARGS = 4;

    // usage
    if( argc < NARGS ||
	sscanf(argv[2], "%d", &uid) != 1 ||
	sscanf(argv[3], "%d", &gid) != 1 ) {
	fprintf(stderr,
		"usage: %s <\"temp\"|\"perm\"> "
		"<targetuid> <targetgid> [targetsups (uniq)]\n",
		argv[0]);
	exit(1);
    }

    // init targetu
    targetu.uid       = uid;
    targetu.gid       = gid;
    targetu.sups.size = 0;
    targetu.sups.list = NULL;

    if( argc > NARGS ) {

	unsigned i, g;
	sups_t *s       = &targetu.sups;
	s->size         = argc - NARGS;
	DO_CHK( s->list = (gid_t*)malloc(s->size*sizeof(gid_t)) );

	for(i=0; i<s->size; i++) {
	    DO_CHK( sscanf(argv[i+NARGS], "%u", &g) == 1 );
	    s->list[i] = g;
	}
	//qsort(s->list, s->size, sizeof(gid_t), cmp_gid);
    }

    // print headeer
    printf("======================== START ==================\n");
    DO_SYS( get_pcred(&p1) );
    origu.uid  = geteuid();
    origu.gid  = getegid();
    origu.sups = p1.sups;
    print_ucred("origu"   , &origu);
    print_ucred("targetu" , &targetu);
    printf("-------------------------------------------------------\n");
    print_pcred("p start", &p1);

    // do work
    if( strcmp("temp",argv[1]) == 0 ) {

	DO_SYS( drop_privileges_temporarily(&targetu) );
	DO_SYS( get_pcred(&p2) );
	print_pcred("p after temp", &p2);

	DO_SYS( restore_privileges(&origu) );
	DO_SYS( get_pcred(&p3) );
	print_pcred("p after rest", &p3);
    }
    else if( strcmp("perm",argv[1]) == 0 ) {
	DO_SYS( drop_privileges_permanently(&targetu) );
	DO_SYS( get_pcred(&p2) );
	print_pcred("p after perm", &p2);
    }
    else
	fprintf(stderr, "usage: first arg must be temp|perm\n");

    return 0;
}
#endif /*TEST_PRIV*/


//##############################################################################
//                                   EOF
//##############################################################################
