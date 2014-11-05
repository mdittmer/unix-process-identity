//##############################################################################
// priv.h
//     Securely change process identity
//     by Dan Tsafrir and David Wagner
// See also
//     http://code.google.com/p/change-process-identity
//     http://www.usenix.org/publications/login/2008-06/pdfs/tsafrir.pdf
//##############################################################################
#ifndef PRIV_H__
#define PRIV_H__


#include <stdbool.h>            /* bool */
#include <sys/types.h>		/* uid_t, gid_t */


// Encapsulate user credentials.
typedef struct supplementary_groups {
    gid_t *list; // must be sorted ascending & unique (elements appear once)
    int    size; // number of elements in 'list' (can be zero)
} sups_t;
typedef struct user_credentials { // the target credentials of a user
    uid_t  uid;
    gid_t  gid;
    sups_t sups;
} ucred_t;


// Permanently change identity of a privileged process (root or setuid)
// to that of the given non-privileged user.
int drop_privileges_permanently(const ucred_t *uc/*target identity*/);


// Switch to the given user, but maintain the ability to regain the
// current effective identity.
int drop_privileges_temporarily(const ucred_t *uc/*target identity*/);


// The reverse operation of drop_privileges_temporarily().
int restore_privileges(const ucred_t *uc/*target identity*/);

// Modified by mdittmer: exported from internal protocol
typedef struct user_ids  { uid_t r, e, s; } uids_t; // real, effective, saved
typedef struct group_ids { gid_t r, e, s; } gids_t;
typedef struct process_credentials {
    uids_t uids;
    gids_t gids;
    sups_t sups;
} pcred_t;
int get_pcred(pcred_t *pc);
bool ucred_is_sane(const ucred_t *uc);
int  set_sups(const sups_t *targetsups);
void print_sups(const char *title, const sups_t  *s);
void print_ucred(const char *title, const ucred_t *u);
void print_pcred(const char *title, const pcred_t *p);

// Changed by mdittmer: Exported for reuse in priv2.c
int get_sups(sups_t *s);
bool eql_sups(const sups_t *cursups, const sups_t *targetsups);

#endif /* PRIV_H__ */
//##############################################################################
//                                   EOF
//##############################################################################
