/*
 * AFS quota groking routines
 *   Must be separated out, because the NFS code and the AFS code don't like
 *   to be in the same file, due to conflicting RPC include files
 */

#include "attach.h"

#include <afs/param.h>
#include <afs/venus.h>
#include <afs/afsint.h>

extern int uflag,gflag,fsind;
extern char *fslist[];
extern int heading_printed;

#define user_and_groups (!uflag && !gflag)

void *
getafsquota(ap, explicit)
    struct _attachtab *ap;
    int explicit;
{
    static struct VolumeStatus vs;
    struct ViceIoctl ibuf;
    int code;

    ibuf.out_size=sizeof(struct VolumeStatus);
    ibuf.in_size=0;
    ibuf.out=(caddr_t) &vs;
    code = pioctl(ap->mntpt,VIOCGETVOLSTAT,&ibuf,1);
    if (code) {
	if (explicit || ((errno != EACCES) && (errno != EPERM))) {
	    fprintf(stderr, "Error getting AFS quota: ");
	    perror(ap->mntpt);
	}
	return(NULL);
    }
    return((void *)&vs);
}

void
prafsquota(ap,foo,heading_id,heading_name)
    struct _attachtab *ap;
    void *foo;
    int heading_id;
    char *heading_name;
{
    struct VolumeStatus *vs;
    char *cp;

    vs = (struct VolumeStatus *) foo;
    cp = ap->mntpt;
    if (!user_and_groups) {
	if (!heading_printed) simpleheading(heading_id,heading_name);
	if (strlen(ap->mntpt) > 15){
	    printf("%s\n",cp);
	    cp = "";
	}
	printf("%-14s %5d%7d%7d%12s\n",
	       cp,
	       vs->BlocksInUse,
	       vs->MaxQuota,
	       vs->MaxQuota,
	       (vs->MaxQuota && (vs->BlocksInUse >= vs->MaxQuota)
		? "Expired" : ""));
    } else {
	if (!heading_printed) heading(heading_id,heading_name);
	if (strlen(cp) > 15){
	    printf("%s\n", cp);
	    cp =  "";
	}
	printf("%-15s %-17s %6d %6d %6d%-2s\n",
	       cp, "volume",
	       vs->BlocksInUse,
	       vs->MaxQuota,
	       vs->MaxQuota,
	       (vs->MaxQuota && (vs->BlocksInUse >= vs->MaxQuota)
		? "<<" : ""));
    }
}

void
afswarn(ap,foo,name)
    struct _attachtab *ap;
    void *foo;
    char *name;
{
    struct VolumeStatus *vs;
    char buf[1024];
    int i;
    uid_t uid=getuid();

    vs = (struct VolumeStatus *) foo;
/*
 * Some sort of heuristic is need to avoid printing quota warnings for
 * lockers the user has no control over.  For now, only print warnings if
 * the locker was attached with mode "w"; it is (somewhat) unlikely that a
 * user will be writing to a locker they do not have write permission to,
 * Additionally, only warn the user if if it was they who attached it, or if
 * they are explicitly asking for information on that locker.
 */

  if (vs->MaxQuota && (vs->BlocksInUse >= vs->MaxQuota)) {
    if (fsind) {
      for(i=0;i<fsind;i++) {
	if(!strcmp(fslist[i],ap->mntpt)) {
	  sprintf(buf,"User %s over disk quota on %s, remove %dK\n", name,
		  ap->mntpt, (vs->BlocksInUse - vs->MaxQuota));
	  putwarning(buf);
	  return;
	}
      }
    }
    else if (ap->mode == 'w') {
      for (i = 0; i < ap->nowners; i++) {
	if (uid != ap->owners[i])
	  continue;
	sprintf(buf,"User %s over disk quota on %s, remove %dK\n", name,
		ap->mntpt, (vs->BlocksInUse - vs->MaxQuota));
	putwarning(buf);
	return;
      }
    }
  }
  return;
}