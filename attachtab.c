/*
 * $Id: attachtab.c,v 1.3 1992-04-10 20:23:43 probe Exp $
 *
 * Copyright (c) 1989,1991 by the Massachusetts Institute of Technology.
 *
 * Contains the routines which access the attachtab file.
 */

#ifndef lint
static char rcsid_attachtab_c[] = "$Header: /afs/dev.mit.edu/source/repository/athena/bin/quota/attachtab.c,v 1.3 1992-04-10 20:23:43 probe Exp $";
#endif lint

#include "attach.h"
#include <sys/file.h>
#include <pwd.h>
#include <string.h>

#define TOKSEP " \t\r\n"

static int parse_attach();

struct	_attachtab	*attachtab_first, *attachtab_last;

extern char *sys_errlist[];
static int debug_flag = 0;
static char *attachtab_fn = "/usr/tmp/attachtab";
static char *abort_msg = "quota: aborting\n";

/*
 * LOCK the attachtab - wait for it if it's already locked
 */
static int attach_lock_fd = -1;
static int attach_lock_count = 0;

void lock_attachtab()
{
	register char	*lockfn;
	
	if (debug_flag)
		printf("Locking attachtab....");
	if (attach_lock_count == 0) {
		if (!(lockfn = malloc(strlen(attachtab_fn)+6))) {
			fprintf(stderr,
				"Can't malloc for lockfile filename\n");
			fprintf(stderr, abort_msg);
			exit(ERR_FATAL);
		}
		(void) strcpy(lockfn, attachtab_fn);
		(void) strcat(lockfn, ".lock");
	
		if (attach_lock_fd < 0) {
			attach_lock_fd = open(lockfn, O_CREAT, 0644);
			if (attach_lock_fd < 0) {
				fprintf(stderr,"Can't open %s: %s\n", lockfn,
					sys_errlist[errno]);
				fprintf(stderr, abort_msg);
				exit(ERR_FATAL);
			}
		}
		flock(attach_lock_fd, LOCK_EX);
		free(lockfn);
	}
	attach_lock_count++;
}


/*
 * UNLOCK the attachtab
 */
void unlock_attachtab()
{
	if (debug_flag)
		printf("Unlocking attachtab\n");
	attach_lock_count--;
	if (attach_lock_count == 0) {
		close(attach_lock_fd);
		attach_lock_fd = -1;
	} else if (attach_lock_count < 0) {
		fprintf(stderr,
	"Programming botch!  Tried to unlock unlocked attachtab\n");
	}
}

/*
 * get_attachtab - Reads in the attachtab file and sets up the doubly
 * 	linked list of attachtab entries.  Assumes attachtab is
 * 	already locked.
 */
void get_attachtab()
{
	register struct _attachtab	*atprev = NULL;
	register struct _attachtab	*at = NULL;
	register FILE			*f;
	char				attach_buf[BUFSIZ];
	
	free_attachtab();
	if ((f = fopen(attachtab_fn, "r")) == NULL)
		return;
	
	while (fgets(attach_buf, sizeof attach_buf, f)) {
		if (!(at = (struct _attachtab *) malloc(sizeof(*at)))) {
			fprintf(stderr,
				"Can't malloc while parsing attachtab\n");
			fprintf(stderr, abort_msg);
			exit(ERR_FATAL);
		}
		parse_attach(attach_buf, at, 0);
		at->prev = atprev;
		if (atprev)
			atprev->next = at;
		else
			attachtab_first = at;
		atprev = at;
	}
	attachtab_last = at;
	fclose(f);
}

/*
 * free the linked list of attachtab entries
 */
void free_attachtab()
{
	register struct _attachtab     	*at, *next;
	
	if (!(at = attachtab_first))
		return;
	while (at) {
		next = at->next;
		free(at);
		at = next;
	}
	attachtab_first = attachtab_last = NULL;
}	

#if 0
/*
 * put_attachtab - write out attachtab file from linked list and
 * 	unlocks attachtab
 */
put_attachtab()
{
	register FILE	*f;
	register struct	_attachtab	*at;
	register char	*tempfn;
	
	if (!(tempfn = malloc(strlen(attachtab_fn)+6))) {
		fprintf(stderr,
			"Can't malloc while writing attachtab.\n");
		fprintf(stderr, abort_msg);
		exit(ERR_FATAL);
	}
	(void) strcpy(tempfn, attachtab_fn);
	(void) strcat(tempfn, ".temp");
	if (!(f = fopen(tempfn, "w"))) {
		fprintf(stderr,"Can't open %s for write: %s\n", attachtab_fn,
			sys_errlist[errno]);
		fprintf(stderr, abort_msg);
		exit(ERR_FATAL);
	}
	at = attachtab_first;
	while (at) {
		register int i;

		fprintf(f, "%s %c%c%s %s %s %s %s",
			at->version, at->explicit ? '1' : '0',
			at->status, (at->fs) ? at->fs->name : "---",
			at->hesiodname, at->host, at->hostdir,
			inet_ntoa(at->hostaddr[0]));
		for (i=1; i<MAXHOSTS && at->hostaddr[i].s_addr; i++)
			fprintf(f, ",%s", inet_ntoa(at->hostaddr[i]));
		fprintf(f, " %d %s %d ",
			at->rmdir, at->mntpt, at->flags);
		if (at->nowners)
			fprintf(f, "%d", at->owners[0]);
		else
			fprintf(f, ",");
		for (i=1; i < at->nowners; i++)
			fprintf(f, ",%d", at->owners[i]);
		fprintf(f, " %d %c\n", at->drivenum, at->mode);

		at = at->next;
	}
	fclose(f);

	if (rename(tempfn, attachtab_fn)) {
		fprintf(stderr,"Can't rename %s to %s\n", tempfn,
			attachtab_fn);
		fprintf(stderr, abort_msg);
		exit(ERR_FATAL);
	}
}

void lint_attachtab()
{
	register struct _attachtab	*atprev = NULL;
	register struct _attachtab	*at = NULL;
	FILE				*f;
	char				attach_buf[BUFSIZ];
	
	free_attachtab();
	if ((f = fopen(attachtab_fn, "r")) == NULL)
		return;
	
	while (fgets(attach_buf, sizeof attach_buf, f)) {
		if (!(at = (struct _attachtab *) malloc(sizeof(*at)))) {
			fprintf(stderr,
				"Can't malloc while parsing attachtab\n");
			fprintf(stderr, abort_msg);
			exit(ERR_FATAL);
		}
		if(parse_attach(attach_buf, at, 1) == FAILURE)
		  continue;	/* bad line -- ignore */
		at->prev = atprev;
		if (atprev)
			atprev->next = at;
		else
			attachtab_first = at;
		atprev = at;
	}
	attachtab_last = at;
	fclose(f);
	put_attachtab();
	free_attachtab();
}
#endif

/*
 * Convert an attachtab line to an attachtab structure
 */
static int parse_attach(bfr, at, lintflag)
    register char *bfr;
    register struct _attachtab *at;
    int lintflag;
{
    register char *cp;
    register int i;

    if (!*bfr)
	goto bad_line;
	
    if (bfr[strlen(bfr)-1] < ' ')
	bfr[strlen(bfr)-1] = '\0';
	
    if (!(cp = strtok(bfr, TOKSEP)))
	    goto bad_line;

    if (strcmp(cp, ATTACH_VERSION)) {
	    fprintf(stderr, "Bad version number in %s\n", attachtab_fn);
	    if(lintflag) 
	      return FAILURE;
	    fprintf(stderr, abort_msg);
	    exit(ERR_FATAL);
    }
    strcpy(at->version, cp);
    
    if (!(cp = strtok(NULL, TOKSEP)))
	    goto bad_line;

    if (*cp != '0' && *cp != '1')
	goto bad_line;
    at->explicit = *cp++-'0';
    
    at->status = *cp++;
    if (at->status != STATUS_ATTACHED && at->status != STATUS_ATTACHING &&
	at->status != STATUS_DETACHING)
	goto bad_line;

    at->fs = get_fs(cp);
    if (at->fs == NULL)
	goto bad_line;

    if (!(cp = strtok(NULL, TOKSEP)))
	    goto bad_line;
    (void) strcpy(at->hesiodname, cp);

    if (!(cp = strtok(NULL, TOKSEP)))
	    goto bad_line;
    (void) strcpy(at->host, cp);

    if (!(cp = strtok(NULL, TOKSEP)))
	    goto bad_line;
    (void) strcpy(at->hostdir, cp);

    if (!(cp = strtok(NULL, TOKSEP)))
	    goto bad_line;

    for (i=0; cp; i++) {
	    register char *dp;

	    if (dp = index(cp, ','))
		    *dp++ = '\0';
	    at->hostaddr[i].s_addr = inet_addr(cp);
	    cp = dp;
    }

    if (!(cp = strtok(NULL, TOKSEP)))
	    goto bad_line;
    at->rmdir = atoi(cp);

    if (!(cp = strtok(NULL, TOKSEP)))
	    goto bad_line;
    (void) strcpy(at->mntpt, cp);

    if (!(cp = strtok(NULL, TOKSEP)))
	    goto bad_line;
    at->flags = atoi(cp);
    
    if (!(cp = strtok(NULL, TOKSEP)))
	    goto bad_line;
    at->nowners = 0;
    if (*cp == ',')	/* If heading character is comma */
	    cp = 0;	/* we have a null list*/
    while (cp) {
	    register char *dp;
	    
	    if (dp = index(cp, ','))
		    *dp++ = '\0';
	    at->owners[at->nowners++] = atoi(cp);
	    cp = dp;
    }
    
    if (!(cp = strtok(NULL, TOKSEP)))
	    goto bad_line;
    
    at->drivenum = atoi(cp);
    
    if (!(cp = strtok(NULL, TOKSEP)))
	    goto bad_line;
    at->mode = *cp;
#if 0
    if (at->fs->good_flags) {
	    if (!index(at->fs->good_flags, at->mode))
		    goto bad_line;	/* Bad attach mode */
    }
#endif
    
    at->next = NULL;
    at->prev = NULL;
    return SUCCESS;
    
bad_line:
    fprintf(stderr,"Badly formatted entry in %s\n", attachtab_fn);
    if (lintflag)
      return FAILURE;
    fprintf(stderr, abort_msg);
    exit(ERR_FATAL);
    /* NOTREACHED */
}

#if 0
/*
 * Routine to see if a host is in the attachtab - uses the linked list
 * attachtab in memory
 */
host_occurs(name)
    register char *name;
{
	register struct _attachtab	*p;

	p = attachtab_first;
	while (p) {
		if (!strcasecmp(p->host, name))
			return(1);
		p = p->next;
	}
	return (0);
}
#endif

/*
 * Lookup an entry in attachtab  --- uses the linked list stored in memory
 */
struct _attachtab *attachtab_lookup(hesiod)
    register char *hesiod;
{
	register struct _attachtab	*p;

	p = attachtab_first;
	while (p) {
		if (!strcmp(p->hesiodname, hesiod))
			return(p);
		p = p->next;
	}
	return (NULL);
}

/*
 * Lookup an entry in attachtab by mountpoint --- uses the linked list
 * 	stored in memory
 */
struct _attachtab *attachtab_lookup_mntpt(pt)
    register char *pt;
{
	register struct _attachtab	*p;

	p = attachtab_first;
	while (p) {
		if (!strcmp(p->mntpt, pt))
			return(p);
		p = p->next;
	}
	return (NULL);
}

#if 0
/*
 * Delete an entry from the attachtab --- unlinks the passed attachtab
 * 	record in the in-core linked list; assumes attachtab is locked.
 * 	You must call put_attachtab to actually affect the copy on disk.
 */
attachtab_delete(at)
	register struct _attachtab	*at;
{
	if (!at)
		return;
	
	if (at->prev)
		at->prev->next = at->next;
	else
		attachtab_first = at->next;
	if (at->next)
		at->next->prev = at->prev;
	else
		attachtab_last = at->prev;
	free(at);
}

/*
 * Append an entry to the linked list database in attachtab
 */
attachtab_append(at)
	register struct _attachtab *at;
{

	register struct _attachtab *atnew;

	if (!(atnew = (struct _attachtab *)
	      malloc(sizeof(struct _attachtab)))) {
		fprintf(stderr, "Can't malloc while adding to attachtab\n");
		fprintf(stderr, abort_msg);
		exit(ERR_FATAL);
	}
	*atnew = *at;

	strcpy(atnew->version, ATTACH_VERSION);
	
	if (attachtab_last)
		attachtab_last ->next = atnew;
	else
		attachtab_first = atnew;
	atnew->prev = attachtab_last;
	atnew->next = NULL;
	attachtab_last = atnew;
}

/*
 * Replace an entry in the linked list 
 */
attachtab_replace(at)
	register struct _attachtab *at;
{
	register struct _attachtab *p, *next, *prev;

	strcpy(at->version, ATTACH_VERSION);
	
	p = attachtab_first;

	while (p) {
		if (!hescmp(p, at->hesiodname) && p->explicit == explicit) {
			next = p->next;
			prev = p->prev;
			*p = *at;
			p->next = next;
			p->prev = prev;
		}
		p = p->next;
	}
}
#endif


struct _fstypes fstypes[] = {
    "NFS", TYPE_NFS,
    "RVD", TYPE_RVD,
    "UFS", TYPE_UFS,
    "ERR", TYPE_ERR,
    "AFS", TYPE_AFS,
    "MUL", TYPE_MUL,
    0, 0 };

/*
 * Convert a string type to a filesystem type entry
 */
struct _fstypes *get_fs(s)
    char *s;
{
    int i;

    if (s && *s) {
	    for (i=0;fstypes[i].name;i++) {
		    if (!strcasecmp(fstypes[i].name, s))
			    return (&fstypes[i]);
	    }
    }
    return (NULL);
}
