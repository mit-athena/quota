/*
 *   Disk quota reporting program.
 *
 *   $Author jnrees $
 *   $Header: /afs/dev.mit.edu/source/repository/athena/bin/quota/quota.c,v 1.11 1991-01-14 13:45:43 epeisach Exp $
 *   $Log: not supported by cvs2svn $
 * Revision 1.10  90/08/07  19:39:32  probe
 * Changes to prevent the super-user from accesing a user's quota on a server
 * running the old RPC daemon, fix formatting problems for long-named
 * filesystems, prevent printing of quota information the user "root" or the
 * group "wheel" which are not quota-controlled, fixed core-dump that would
 * happen if warnings were to be printed for multiple users.  [jnrees]
 * 
 * Revision 1.9  90/07/17  09:12:43  epeisach
 * Quota changes from jnrees for 7.1
 * 
 * Revision 1.8  90/06/01  15:20:40  jnrees
 * Fixed core dump error, again.   This time it was found to 
 * dump core if a warning was to be printed for a user or group
 * which was not known on the workstation. Previously I only
 * fixed the part which was for printing out all values (-v flag).
 * 
 * 
 * Revision 1.7  90/05/24  10:58:11  jnrees
 * Fixed potential bus error problem, dereferencing an unset pointer.
 * 
 * Revision 1.6  90/05/23  12:25:41  jnrees
 * Changed output format.  '-i' flag no longer needed.
 * Added a usage message if command line is improper.
 * 
 * Revision 1.5  90/05/22  12:12:09  jnrees
 * Fixup of permissions, plus fallback to old rpc call
 * if the first call fails because the rpc server is not
 * registered on the server machine.
 * 
 * Revision 1.4  90/05/17  15:04:02  jnrees
 * Fixed bug where unknown uid or gid would result in a bus error
 * 
 *   
 *   Uses the rcquota rpc call for group and user quotas
 */
#include <stdio.h>
#ifndef ultrix
#include <mntent.h>
#else
#include <sys/types.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <fstab.h>
#include <sys/fs_types.h>
#include <sys/stat.h>
#define mntent fs_data
struct fs_data mountbuffer[NMOUNT];
#endif
#include <ctype.h>
#include <pwd.h>
#include <grp.h>

#include <sys/param.h>
#include <sys/file.h>
#include <sys/time.h>
#ifdef ultrix
#include <sys/quota.h>
#else
#include <ufs/quota.h>
#endif
#ifndef ultrix
#include <qoent.h>
#endif

#include <rpc/rpc.h>
#include <rpc/pmap_prot.h>
#include <sys/socket.h>
#include <netdb.h>
#include <rpcsvc/rquota.h>
#include <rpcsvc/rcquota.h>

static char *warningstring = NULL;

static int	vflag=0, uflag=0, gflag=0;
#ifdef ultrix
int	qflag=0, done=0;
#endif

#define user_and_groups (!uflag && !gflag)
#define QFNAME	"quotas"

#define kb(n)   (howmany(dbtob(n), 1024))

#define MAXFS 16
#define MAXID 32
/* List of id's and filesystems to check */
char *fslist[MAXFS], *idlist[MAXID];
int fsind = 0, idind = 0, heading_printed;

main(argc, argv)
     char *argv[];
{
  register char *cp;
  register int i;

  argc--,argv++;
  while (argc > 0) {
    if (argv[0][0] == '-')
      for (cp = &argv[0][1]; *cp; cp++) switch (*cp) {

      case 'v':
	vflag=1;
	break;

#ifdef ultrix
    case 'q':
	qflag=1;
	break;
#endif

      case 'g':
	gflag=1;
	break;

      case 'u':
	uflag=1;
	break;

      case 'f':
	if (fsind < MAXFS)
	  {
	    if (argc == 1){
	      fprintf(stderr, "quota: No filesystem argument\n");
	      usage();
	      exit(1);
	    }
	    fslist[fsind++] = argv[1];
	    argv++;
	    argc--;
	    break;
	  }
	else{
	  fprintf(stderr, "quota: too many filesystems\n");
	  exit(2);
	}

      default:
	fprintf(stderr, "quota: %c: unknown option\n",
		*cp);
	usage();
	exit(3);
      }
    else if (idind < MAXID)
	{
	  idlist[idind++] = argv[0];
	}
    else{
      fprintf(stderr, "quota: too many id's\n");
      exit(4);
    }
    argv++;
    argc--;
  }
  
  if (gflag && uflag){
    fprintf(stderr, "quota: Can't use both -g and -u\n");
    usage();
    exit(5);
  }

  if (uflag && idind == 0){
    idlist[0] = (char*)malloc(20);
    sprintf(idlist[0],"%d",getuid());
    idind = 1;
  }

  if (gflag  && (idind == 0)){
    fprintf(stderr, "quota: No groups specified\n");
    usage();
    exit(6);
  }

  if (fsind) verify_filesystems();

  if (idind == 0) showid(getuid());
  else
    for(i=0;i<idind;i++){
      heading_printed = 0;
      if ((!gflag && getpwnam(idlist[i])) ||
	  (gflag && getgrnam(idlist[i])))
	showname(idlist[i]);
      else if (alldigits(idlist[i])) showid(atoi(idlist[i]));
      else showname(idlist[i]);
    }

  exit(0);
}

showid(id)
     int id;
{
  register struct passwd *pwd = getpwuid(id);
  register struct group  *grp = getgrgid(id);

  if (id == 0){
    if (vflag) printf("no disk quota for %s 0\n",
		      (gflag ? "gid" : "uid"));
    return;
  }

  if (gflag){
    if (grp == NULL) showquotas(id, "(no group)");
    else showquotas(id, grp->gr_name);
  }
  else{ 
    if (pwd == NULL) showquotas(id, "(no account)");
    else showquotas(id, pwd->pw_name);
  }
  return;
}

showname(name)
     char *name;
{
  register struct passwd *pwd = getpwnam(name);
  register struct group  *grp = getgrnam(name);

  if (gflag){
    if (grp == NULL){
      fprintf(stderr, "quota: %s: unknown group\n", name);
      exit(7);
      return;
    }
    if (grp->gr_gid == 0){
      if (vflag) printf("no disk quota for %s (gid 0)\n", name);
      return;
    }
    showquotas(grp->gr_gid,name);
  }
  else{
    if (pwd == NULL){
      fprintf(stderr, "quota: %s: unknown user\n", name);
      exit(8);
    }
    if (pwd->pw_uid == 0){
      if (vflag)
	printf("no disk quota for %s (uid 0)\n", name);
      return;
    }
    showquotas(pwd->pw_uid,name);
  }
}

showquotas(id,name)
     int id;
     char *name;
{
  register struct mntent *mntp;
#ifdef ultrix
#define mnt_dir fd_path
#define mnt_type fd_fstype
#define mnt_fsname fd_devname
#define dqb_fhardlimit dqb_bhardlimit
#define dqb_fsoftlimit dqb_bsoftlimit
#define dqb_curfiles dqb_curinodes
#define MNTTYPE_42 GT_ULTRIX
#define MNTTYPE_NFS GT_NFS
#define MNTOPT_QUOTA MOUNT_QUOTA
#define Q_GETQUOTA Q_GETDLIM
#endif
  FILE *mtab;
  int myuid, ngroups, gidset[NGROUPS];
  struct getcquota_rslt qvalues;
#ifdef ultrix
  int loc=0, ret;
  int ultlocalquotas=0;
#endif

  myuid = getuid();

  if (gflag){ /* User must be in group or be the super-user */
    if ((ngroups = getgroups(NGROUPS, gidset)) == -1){
      perror("quota: Couldn't get list of groups.");
      exit(9);
    }
    while(ngroups){ if (id == gidset[ngroups-1]) break; --ngroups;}
    if (!ngroups && myuid != 0){
      printf("quota: %s (gid %d): permission denied\n", name, id);
      return;
    }
  }
  else{
    if (id != myuid && myuid != 0){
      printf("quota: %s (uid %d): permission denied\n", name, id);
      return;
    }
  }

#ifndef ultrix
  mtab = setmntent(MOUNTED, "r");
#else
  ret = getmountent(&loc, mountbuffer, NMOUNT);
  if (ret == 0) {
      perror("getmountent");
      exit(3);
  }
#endif

#ifndef ultrix
  while(mntp = getmntent(mtab)){
#else
  for(mntp = mountbuffer; mntp < &mountbuffer[ret]; mntp++) {
#endif
    if (fsind)
      {
	int i, l;
	for(i=0;i<fsind;i++){
	  l = strlen(fslist[i]);
	  if(!strncmp(mntp->mnt_dir, fslist[i], l))
	    break;
	}
	if (i == fsind) continue; /* If this filesystem isn't in the fslist,
				     punt.*/
      }
        
#ifndef ultrix
    if (strcmp(mntp->mnt_type, MNTTYPE_42) == 0 &&
	hasmntopt(mntp, MNTOPT_QUOTA)){
      if (getlocalquota(mntp,id,&qvalues)) continue;
#else
    if (mntp->mnt_type == MNTTYPE_42 &&
        (mntp->fd_flags & M_QUOTA)) {
	ultlocalquotas++;
      continue;
#endif
      }
#ifndef ultrix
    else if (strcmp(mntp->mnt_type, MNTTYPE_NFS) == 0){
#else
    else if (mntp->mnt_type == MNTTYPE_NFS){
#endif
      if (!getnfsquota(mntp, id, &qvalues)) continue;
    }
    else continue;

    if (vflag) prquota(mntp, &qvalues, id, name);      
    if (user_and_groups || !vflag) warn(mntp, &qvalues);
  }
#ifndef ultrix
  endmntent(mtab);
#endif
  if (warningstring){
    printf("\n%s\n", warningstring);
    free(warningstring);
    warningstring = NULL;
  }
#ifdef ultrix
  if(ultlocalquotas) 
      ultprintquotas(id,name);
#endif
}

#ifndef ultrix
getlocalquota(mntp, uid, qvp)
     struct mntent *mntp;
     int uid;
     struct getcquota_rslt *qvp;
{
  struct qoent *qoent;
  FILE *qotab;
  struct dqblk dqblk;

  /* First get the options */
  qotab = setqoent(QOTAB);
  while(qoent = getqoent(qotab)){
    if (!strcmp(mntp->mnt_dir, qoent->mnt_dir))
      break;
    else continue;
  }

  if (!qoent){ /* this partition has no quota options, use defaults */
    qvp->rq_group = !strcmp(QOTYPE_DEFAULT, QOTYPE_GROUP);
    qvp->rq_bsize = DEV_BSIZE;
    qvp->gqr_zm.rq_bhardlimit = QO_BZHL_DEFAULT;
    qvp->gqr_zm.rq_bsoftlimit = QO_BZSL_DEFAULT;
    qvp->gqr_zm.rq_fhardlimit = QO_FZHL_DEFAULT;
    qvp->gqr_zm.rq_fsoftlimit = QO_FZSL_DEFAULT;
  }
  else{
    /* qoent contains options */
    qvp->rq_group = QO_GROUP(qoent);
    qvp->rq_bsize = DEV_BSIZE;
    qvp->gqr_zm.rq_bhardlimit = qoent->qo_bzhl;
    qvp->gqr_zm.rq_bsoftlimit = qoent->qo_bzsl;
    qvp->gqr_zm.rq_fhardlimit = qoent->qo_fzhl;
    qvp->gqr_zm.rq_fsoftlimit = qoent->qo_fzsl;
  }

  /* If this filesystem is group controlled and user quota is being
     requested, or this is a user controlled filesystem and group
     quotas are requested, then punt */
  if ((qvp->rq_group && uflag) || 
      (!qvp->rq_group && gflag))
    return(-1);

  if (uflag || gflag || !qvp->rq_group){
    if (quotactl(Q_GETQUOTA, mntp->mnt_fsname, uid, &dqblk)){
      /* ouch! quotas are on, but this failed */
      fprintf(stderr, "quotactl: %s %d\n", mntp->mnt_fsname, uid);
      return(-1);
    }

    qvp->rq_ngrps = 1;
    dqblk2rcquota(&dqblk,&(qvp->gqr_rcquota[0]), uid);
    return(0);
  }
  else{
    int groups[NGROUPS], i;
    
    qvp->rq_ngrps = getgroups(NGROUPS,groups);
    for(i=0;i<qvp->rq_ngrps;i++){
      if (quotactl(Q_GETQUOTA, mntp->mnt_fsname, groups[i], &dqblk)){
	/* ouch again! */
	fprintf(stderr, "quotactl: %s %d\n", mntp->mnt_fsname, groups[i]);
	return(-1);
      }
      dqblk2rcquota(&dqblk, &(qvp->gqr_rcquota[i]), groups[i]);
    }
    return(0);
  }
}
#endif

int
getnfsquota(mntp, uid, qvp)
     struct mntent *mntp;
     int uid;
     struct getcquota_rslt *qvp;
{
  char *hostp;
  char *cp;
  struct getcquota_args gq_args;
  extern char *index();
  int oldrpc = 0;

  hostp = mntp->mnt_fsname;
  cp = index(mntp->mnt_fsname, ':');
  if (cp == 0) {
    fprintf(stderr, "cannot find hostname for %s\n", mntp->mnt_dir);
    return (0);
  }
  *cp = '\0';
  gq_args.gqa_pathp = cp + 1;
  gq_args.gqa_uid = (gflag ? getuid() : uid);
  if ((enum clnt_stat)
      callrpc(hostp, RCQUOTAPROG, RCQUOTAVERS,
	      (vflag? RCQUOTAPROC_GETQUOTA: RCQUOTAPROC_GETACTIVEQUOTA),
	      xdr_getcquota_args, &gq_args, xdr_getcquota_rslt, qvp) ==
      RPC_PROGNOTREGISTERED){

    /* Fallback on old rpc, unless gflag is true, or caller is
     the superuser. */
    struct getquota_rslt oldquota_result;

    if (gflag || !getuid()) return(0);
    oldrpc = 1;
    if (callaurpc(hostp, RQUOTAPROG, RQUOTAVERS,
		  (vflag? RQUOTAPROC_GETQUOTA:
		   RQUOTAPROC_GETACTIVEQUOTA),
		  xdr_getquota_args, &gq_args,
		  xdr_getquota_rslt, &oldquota_result) != 0){
      /* Okay, it really failed */
      *cp = ':';
      return (0);
    }
    else{
      /* The getquota_rslt structure needs to be converted to
	 a getcquota_rslt structure*/

      switch (oldquota_result.gqr_status){
      case Q_OK: qvp->gqr_status = QC_OK; break;
      case Q_NOQUOTA: qvp->gqr_status = QC_NOQUOTA; break;
      case Q_EPERM: qvp->gqr_status = QC_EPERM; break;
      }

      qvp->rq_group = 0;	/* only user quota on old rpc's */
      qvp->rq_ngrps = 1;
      qvp->rq_bsize = oldquota_result.gqr_rquota.rq_bsize;
      bzero(&qvp->gqr_zm, sizeof(struct rcquota));

      qvp->gqr_rcquota[0].rq_id = gq_args.gqa_uid;
      qvp->gqr_rcquota[0].rq_bhardlimit =
	oldquota_result.gqr_rquota.rq_bhardlimit;
      qvp->gqr_rcquota[0].rq_bsoftlimit =
	oldquota_result.gqr_rquota.rq_bsoftlimit;
      qvp->gqr_rcquota[0].rq_curblocks =
	oldquota_result.gqr_rquota.rq_curblocks;
      qvp->gqr_rcquota[0].rq_fhardlimit =
	oldquota_result.gqr_rquota.rq_fhardlimit;
      qvp->gqr_rcquota[0].rq_fsoftlimit =
	oldquota_result.gqr_rquota.rq_fsoftlimit;
      qvp->gqr_rcquota[0].rq_curfiles = 
	oldquota_result.gqr_rquota.rq_curfiles;
      qvp->gqr_rcquota[0].rq_btimeleft = 
	oldquota_result.gqr_rquota.rq_btimeleft;
      qvp->gqr_rcquota[0].rq_ftimeleft = 
	oldquota_result.gqr_rquota.rq_ftimeleft;
    }
  }

  switch (qvp->gqr_status) {
  case QC_OK:
    {
      struct timeval tv;
      int i;
      float blockconv;

      if (gflag){
	if (!qvp->rq_group) return(0); /* Not group controlled */
	for(i=0;i<qvp->rq_ngrps;i++)
	  if (uid == qvp->gqr_rcquota[i].rq_id) break;
	if (i == qvp->rq_ngrps) return(0); /* group id not in list */
	bcopy(&(qvp->gqr_rcquota[i]), &(qvp->gqr_rcquota[0]),
	      sizeof(struct rcquota));
	qvp->rq_ngrps = 1;
      }

      if (uflag && qvp->rq_group) return(0); /* Not user-controlled */

      gettimeofday(&tv, NULL);
      blockconv = (float)qvp->rq_bsize / DEV_BSIZE;
      qvp->gqr_zm.rq_bhardlimit *= blockconv;
      qvp->gqr_zm.rq_bsoftlimit *= blockconv;
      qvp->gqr_zm.rq_curblocks  *= blockconv;
      if (!qvp->rq_group) qvp->rq_ngrps = 1;
      for(i=0;i<qvp->rq_ngrps;i++){
	qvp->gqr_rcquota[i].rq_bhardlimit *= blockconv;
	qvp->gqr_rcquota[i].rq_bsoftlimit *= blockconv;
	qvp->gqr_rcquota[i].rq_curblocks  *= blockconv;
	qvp->gqr_rcquota[i].rq_btimeleft += tv.tv_sec;
	qvp->gqr_rcquota[i].rq_ftimeleft += tv.tv_sec;
      }
      *cp = ':';
      return (1);
    }

  case QC_NOQUOTA:
    break;

  case QC_EPERM:
    if (vflag && fsind && !oldrpc)
      fprintf(stderr, "quota: Warning--no NFS mapping on host: %s\n", hostp);
    if (vflag && fsind && oldrpc)
      fprintf(stderr, "quota: Permission denied. %s\n", hostp);
    break;

  default:
    fprintf(stderr, "bad rpc result, host: %s\n",  hostp);
    break;
  }
  *cp = ':';
  return (0);
}

callaurpc(host, prognum, versnum, procnum, inproc, in, outproc, out)
	char *host;
	xdrproc_t inproc, outproc;
     struct getcquota_args *in;
     struct getquota_rslt *out;
{
	struct sockaddr_in server_addr;
	enum clnt_stat clnt_stat;
	struct hostent *hp;
	struct timeval timeout, tottimeout;

	static CLIENT *client = NULL;
	static int socket = RPC_ANYSOCK;
	static int valid = 0;
	static int oldprognum, oldversnum;
	static char oldhost[256];

	if (valid && oldprognum == prognum && oldversnum == versnum
		&& strcmp(oldhost, host) == 0) {
		/* reuse old client */		
	}
	else {
		valid = 0;
		close(socket);
		socket = RPC_ANYSOCK;
		if (client) {
			clnt_destroy(client);
			client = NULL;
		}
		if ((hp = gethostbyname(host)) == NULL)
			return ((int) RPC_UNKNOWNHOST);
		timeout.tv_usec = 0;
		timeout.tv_sec = 6;
		bcopy(hp->h_addr, &server_addr.sin_addr, hp->h_length);
		server_addr.sin_family = AF_INET;
		/* ping the remote end via tcp to see if it is up */
		server_addr.sin_port =  htons(PMAPPORT);
		if ((client = clnttcp_create(&server_addr, PMAPPROG,
		    PMAPVERS, &socket, 0, 0)) == NULL) {
			return ((int) rpc_createerr.cf_stat);
		} else {
			/* the fact we succeeded means the machine is up */
			close(socket);
			socket = RPC_ANYSOCK;
			clnt_destroy(client);
			client = NULL;
		}
		/* now really create a udp client handle */
		server_addr.sin_port =  0;
		if ((client = clntudp_create(&server_addr, prognum,
		    versnum, timeout, &socket)) == NULL)
			return ((int) rpc_createerr.cf_stat);
		client->cl_auth = authunix_create_default();
		valid = 1;
		oldprognum = prognum;
		oldversnum = versnum;
		strcpy(oldhost, host);
	}
	tottimeout.tv_sec = 25;
	tottimeout.tv_usec = 0;
	clnt_stat = clnt_call(client, procnum, inproc, in,
	    outproc, out, tottimeout);
	/* 
	 * if call failed, empty cache
	 */
	if (clnt_stat != RPC_SUCCESS)
		valid = 0;
	return ((int) clnt_stat);
}


simpleheading(id,name)
     int id;
     char *name;
{
  printf("Disk quotas for %s %s (%s %d):\n",
	 (gflag? "group":"user"), name,
	 (gflag? "gid": "uid"), id);
  printf("%-12s %7s%7s%7s%12s%7s%7s%7s%12s\n"
	 , "Filesystem"
	 , "usage"
	 , "quota"
	 , "limit"
	 , "timeleft"
	 , "files"
	 , "quota"
	 , "limit"
	 , "timeleft"
	 );
  heading_printed = 1;
}

heading(id,name)
     int id;
     char *name;
{
  printf("Disk quotas for %s (uid %d):\n",name,id);
  printf("%-16s%-6s%-12s%6s%7s%7s  %7s%7s%7s\n"
	 , "Filesystem"
	 , "Type"
	 , "ID"
	 , "usage", "quota", "limit"
	 , "files", "quota", "limit"
	 );
  heading_printed = 1;
}

prquota(mntp, qvp, heading_id, heading_name)
     register struct mntent *mntp;
     register struct getcquota_rslt *qvp;
     int heading_id;
     char *heading_name;
{
  struct timeval tv;
  char ftimeleft[80], btimeleft[80], idbuf[20];
  char *cp, *id_name = "", *id_type;
  int i;
  struct rcquota *rqp;

  id_type = (qvp->rq_group? "group" : "user");


  gettimeofday(&tv, NULL);

  for(i=0; i<qvp->rq_ngrps; i++){

    /* If this is root or wheel, then skip */
    if (qvp->gqr_rcquota[i].rq_id == 0) continue;

    rqp = &(qvp->gqr_rcquota[i]);

    /* We're not interested in this group if all is zero */
    if (!rqp->rq_bsoftlimit && !rqp->rq_bhardlimit
        && !rqp->rq_curblocks && !rqp->rq_fsoftlimit
        && !rqp->rq_fhardlimit && !rqp->rq_curfiles) continue;

    if (user_and_groups){
      if (qvp->rq_group){
	getgroupname(qvp->gqr_rcquota[i].rq_id, idbuf);
	id_name = idbuf;
      }
      else{ /* this is a user quota */
	getusername(qvp->gqr_rcquota[i].rq_id, idbuf);
	id_name = idbuf;
      }
    }

    /* Correct for zero quotas... */
    if(!rqp->rq_bsoftlimit)
      rqp->rq_bsoftlimit = qvp->gqr_zm.rq_bsoftlimit;
    if(!rqp->rq_bhardlimit)
      rqp->rq_bhardlimit = qvp->gqr_zm.rq_bhardlimit;
    if(!rqp->rq_fsoftlimit)
      rqp->rq_fsoftlimit = qvp->gqr_zm.rq_fsoftlimit;
    if(!rqp->rq_fhardlimit)
      rqp->rq_fhardlimit = qvp->gqr_zm.rq_fhardlimit;

    if (!rqp->rq_bsoftlimit && !rqp->rq_bhardlimit &&
	!rqp->rq_fsoftlimit && !rqp->rq_fhardlimit)
      /* Skip this entirely for compatibility */
      continue;

    if (rqp->rq_bsoftlimit &&
	rqp->rq_curblocks >= rqp->rq_bsoftlimit) {
      if (rqp->rq_btimeleft == 0) {
	strcpy(btimeleft, "NOT STARTED");
      } else if (rqp->rq_btimeleft > tv.tv_sec) {
	fmttime(btimeleft, rqp->rq_btimeleft - tv.tv_sec);
      } else {
	strcpy(btimeleft, "EXPIRED");
      }
    } else {
      btimeleft[0] = '\0';
    }
    if (rqp->rq_fsoftlimit &&
	rqp->rq_curfiles >= rqp->rq_fsoftlimit) {
      if (rqp->rq_ftimeleft == 0) {
	strcpy(ftimeleft, "NOT STARTED");
      } else if (rqp->rq_ftimeleft > tv.tv_sec) {
	fmttime(ftimeleft, rqp->rq_ftimeleft - tv.tv_sec);
      } else {
	strcpy(ftimeleft, "EXPIRED");
      }
    } else {
      ftimeleft[0] = '\0';
    }

    cp = mntp->mnt_dir;

    if (!user_and_groups){
      if (!heading_printed) simpleheading(heading_id,heading_name);
      if (strlen(cp) > 15){
	printf("%s\n",cp);
	cp = "";
      }
      printf("%-14s %5d%7d%7d%12s%7d%7d%7d%12s\n",
	     cp,
	     kb(rqp->rq_curblocks),
	     kb(rqp->rq_bsoftlimit),
	     kb(rqp->rq_bhardlimit),
	     btimeleft,
	     rqp->rq_curfiles,
	     rqp->rq_fsoftlimit,
	     rqp->rq_fhardlimit,
	     ftimeleft
	     );
    }
    else{
      if (!heading_printed) heading(heading_id,heading_name);
      if (strlen(cp) > 16){
	printf("%s\n", cp);
	cp =  "";
      }
      printf("%-16s%-6s%-12.12s%6d%7d%7d%-2s%7d%7d%7d%-2s\n",
	     cp, id_type, id_name,
	     kb(rqp->rq_curblocks),
	     kb(rqp->rq_bsoftlimit),
	     kb(rqp->rq_bhardlimit),
	     (btimeleft[0]? "<<" : ""),
	     rqp->rq_curfiles,
	     rqp->rq_fsoftlimit,
	     rqp->rq_fhardlimit,
	     (ftimeleft[0]? "<<" : ""));
    }
  }
}
  
warn(mntp, qvp)
     register struct mntent *mntp;
     register struct getcquota_rslt *qvp;
{
  struct timeval tv;
  int i;
  char buf[1024], idbuf[20];
  char *id_name, *id_type;
  struct rcquota *rqp;

  id_type = (qvp->rq_group? "Group" : "User");

  gettimeofday(&tv, NULL);

  for(i=0; i<qvp->rq_ngrps; i++){

    /* If this is root or wheel, then skip */
    if (qvp->gqr_rcquota[i].rq_id == 0) continue;

    if (qvp->rq_group){
      getgroupname(qvp->gqr_rcquota[i].rq_id, idbuf);
      id_name = idbuf;
    }
    else{
      getusername(qvp->gqr_rcquota[i].rq_id, idbuf);
      id_name = idbuf;
    }

    rqp = &(qvp->gqr_rcquota[i]);

    /* Correct for zero quotas... */
    if(!rqp->rq_bsoftlimit)
      rqp->rq_bsoftlimit = qvp->gqr_zm.rq_bsoftlimit;
    if(!rqp->rq_bhardlimit)
      rqp->rq_bhardlimit = qvp->gqr_zm.rq_bhardlimit;
    if(!rqp->rq_fsoftlimit)
      rqp->rq_fsoftlimit = qvp->gqr_zm.rq_fsoftlimit;
    if(!rqp->rq_fhardlimit)
      rqp->rq_fhardlimit = qvp->gqr_zm.rq_fhardlimit;

    /* Now check for over...*/
    if(rqp->rq_bhardlimit &&
       rqp->rq_curblocks >= rqp->rq_bhardlimit){
      sprintf(buf,
	      "Block limit reached for %s %s on %s\n",
	     id_type, id_name, mntp->mnt_dir);
      putwarning(buf);
    }

    else if (rqp->rq_bsoftlimit &&
	     rqp->rq_curblocks >= rqp->rq_bsoftlimit){
      if (rqp->rq_btimeleft == 0) {
	sprintf(buf,
		"%s %s over disk quota on %s, remove %dK\n",
	       id_type, id_name, mntp->mnt_dir,
	       kb(rqp->rq_curblocks - rqp->rq_bsoftlimit + 1));
	putwarning(buf);
      }
      else if (rqp->rq_btimeleft > tv.tv_sec) {
	char btimeleft[80];

	fmttime(btimeleft, rqp->rq_btimeleft - tv.tv_sec);
	sprintf(buf,
		"%s %s over disk quota on %s, remove %dK within %s\n",
	       id_type, id_name, mntp->mnt_dir,
	       kb(rqp->rq_curblocks - rqp->rq_bsoftlimit + 1),
	       btimeleft);
	putwarning(buf);
      }
      else {
	sprintf(buf,
		"%s %s over disk quota on %s, time limit has expired, remove %dK\n",
	       id_type, id_name, mntp->mnt_dir,
	       kb(rqp->rq_curblocks - rqp->rq_bsoftlimit + 1));
	putwarning(buf);
      }
    }

    if (rqp->rq_fhardlimit &&
	rqp->rq_curfiles >= rqp->rq_fhardlimit){
      sprintf(buf,
	      "File count limit reached for %s %s on %s\n",
	     id_type, id_name, mntp->mnt_dir);
      putwarning(buf);
    }

    else if (rqp->rq_fsoftlimit &&
	     rqp->rq_curfiles >= rqp->rq_fsoftlimit) {
      if (rqp->rq_ftimeleft == 0) {
	sprintf(buf,
		"%s %s over file quota on %s, remove %d file%s\n",
	       id_type, id_name, mntp->mnt_dir,
	       rqp->rq_curfiles - rqp->rq_fsoftlimit + 1,
	       ((rqp->rq_curfiles - rqp->rq_fsoftlimit + 1) > 1 ?
		"s" : "" ));
	putwarning(buf);
      }

      else if (rqp->rq_ftimeleft > tv.tv_sec) {
	char ftimeleft[80];

	fmttime(ftimeleft, rqp->rq_ftimeleft - tv.tv_sec);
	sprintf(buf,
		"%s %s over file quota on %s, remove %d file%s within %s\n",
	       id_type,id_name,mntp->mnt_dir,
	       rqp->rq_curfiles - rqp->rq_fsoftlimit + 1,
	       ((rqp->rq_curfiles - rqp->rq_fsoftlimit + 1) > 1 ?
		"s" : "" ), ftimeleft);
	putwarning(buf);
      }
      else {
	sprintf(buf,
		"%s %s over file quota on %s, time limit has expired, remove %d file%s\n",
	       id_type, id_name, mntp->mnt_dir,
	       rqp->rq_curfiles - rqp->rq_fsoftlimit + 1,
	       ((rqp->rq_curfiles - rqp->rq_fsoftlimit + 1) > 1 ?
		"s" : "" ));
	putwarning(buf);
      }
    }
  }
}

usage()
{
  fprintf(stderr,
	"Usage: quota [-v] [user] [-g group] [-u user] [-f filesystem]\n");
}

alldigits(s)
	register char *s;
{
	register c;

	c = *s++;
	do {
		if (!isdigit(c))
			return (0);
	} while (c = *s++);
	return (1);
}

#ifndef ultrix
dqblk2rcquota(dqblkp, rcquotap, uid)
     struct dqblk *dqblkp;
     struct rcquota *rcquotap;
     int uid;
{
  rcquotap->rq_id = uid;
  rcquotap->rq_bhardlimit = dqblkp->dqb_bhardlimit;
  rcquotap->rq_bsoftlimit = dqblkp->dqb_bsoftlimit;
  rcquotap->rq_curblocks  = dqblkp->dqb_curblocks;
  rcquotap->rq_fhardlimit = dqblkp->dqb_fhardlimit;
  rcquotap->rq_fsoftlimit = dqblkp->dqb_fsoftlimit;
  rcquotap->rq_curfiles   = dqblkp->dqb_curfiles;
  rcquotap->rq_btimeleft  = dqblkp->dqb_btimelimit;
  rcquotap->rq_ftimeleft  = dqblkp->dqb_ftimelimit;
}
#endif
    
getgroupname(id,buffer)
     int id;
     char *buffer;
{
  if (getgrgid(id))
    strcpy(buffer, (getgrgid(id))->gr_name);
  else{
    sprintf(buffer, "G%d", id);
  }
}

getusername(id,buffer)
     int id;
     char *buffer;
{
  if (getpwuid(id))
    strcpy(buffer, (getpwuid(id))->pw_name);
  else{
    sprintf(buffer, "#%d", id);
  }
}

putwarning(string)
     char *string;
{
  static warningmaxsize = 0;
  
  if (warningstring == 0){
    warningstring = (char*)malloc(10);
    warningstring[0] = '\0';
    warningmaxsize = 10;
  }

  while (strlen(warningstring) + strlen(string) + 1 > warningmaxsize){
    warningstring = (char*)realloc(warningstring, (warningmaxsize * 3)/2);
    warningmaxsize = (warningmaxsize * 3) / 2;
  }

  sprintf(&warningstring[strlen(warningstring)], "%s", string);
}

fmttime(buf, time)
	char *buf;
	register long time;
{
	int i;
	static struct {
		int c_secs;		/* conversion units in secs */
		char * c_str;		/* unit string */
	} cunits [] = {
		{60*60*24*28, "months"},
		{60*60*24*7, "weeks"},
		{60*60*24, "days"},
		{60*60, "hours"},
		{60, "mins"},
		{1, "secs"}
	};

	if (time <= 0) {
		strcpy(buf, "EXPIRED");
		return;
	}
	for (i = 0; i < sizeof(cunits)/sizeof(cunits[0]); i++) {
		if (time >= cunits[i].c_secs)
			break;
	}
	sprintf(buf, "%.1f %s", (double)time/cunits[i].c_secs, cunits[i].c_str);
}

verify_filesystems()
{
  struct mntent *mntp;
  FILE *mtab;
  int i,l, found;
#ifdef ultrix
  int loc=0, ret;
#endif

  for(i=0;i<fsind;i++){
    l = strlen(fslist[i]);
    found = 0;
#ifndef ultrix
    mtab = setmntent(MOUNTED, "r");
#else
  ret = getmountent(&loc, mountbuffer, NMOUNT);
  if (ret == 0) {
      perror("getmountent");
      exit(3);
  }
#endif

#ifndef ultrix
    while(mntp = getmntent(mtab)){
#else
    for(mntp = mountbuffer; mntp < &mountbuffer[ret]; mntp++) {
#endif
      if (!strncmp(fslist[i], mntp->mnt_dir,l)){
	found = 1;
	break;
      }
    }
#ifndef ultrix
    endmntent(mtab);
#endif
    if (!found){
      fprintf(stderr, "quota: '%s' matches no mounted filesystems.\n",
	      fslist[i]);
      exit(10);
    }
  }
}

#ifdef ultrix
#undef mnt_dir
#undef mnt_type
#undef mnt_fsname
#undef dqb_fhardlimit 
#undef dqb_fsoftlimit 
#undef dqb_curfiles 
ultprintquotas(uid, name)
	int uid;
	char *name;
{
	register char c, *p;
	register struct fstab *fs;
	int myuid;

	myuid = getuid();
	if (uid != myuid && myuid != 0) {
		printf("quota: %s (uid %d): permission denied\n", name, uid);
		return;
	}
	done = 0;
	setfsent();
	while (fs = getfsent()) {
		register char *msgi = (char *)0, *msgb = (char *)0;
		register enab = 1;
		dev_t	fsdev;
		struct	stat statb;
		struct	dqblk dqblk;
		char qfilename[MAXPATHLEN + 1], iwarn[8], dwarn[8];

		if (stat(fs->fs_spec, &statb) < 0)
			continue;
		fsdev = statb.st_rdev;
		(void) sprintf(qfilename, "%s/%s", fs->fs_file, QFNAME);
		if (stat(qfilename, &statb) < 0 || statb.st_dev != fsdev)
			continue;
		if (quota(Q_GETDLIM, uid, fsdev, &dqblk) != 0) {
			register fd = open(qfilename, O_RDONLY);

			if (fd < 0)
				continue;
			lseek(fd, (long)(uid * sizeof (dqblk)), L_SET);
			if (read(fd, &dqblk, sizeof dqblk) != sizeof (dqblk)) {
				close(fd);
				continue;
			}
			close(fd);
			if (dqblk.dqb_isoftlimit == 0 &&
			    dqblk.dqb_bsoftlimit == 0)
				continue;
			enab = 0;
		}
		if (dqblk.dqb_ihardlimit &&
		    dqblk.dqb_curinodes >= dqblk.dqb_ihardlimit)
			msgi = "File count limit reached on %s";
		else if (enab && dqblk.dqb_iwarn == 0)
			msgi = "Out of inode warnings on %s";
		else if (dqblk.dqb_isoftlimit &&
		    dqblk.dqb_curinodes >= dqblk.dqb_isoftlimit)
			msgi = "Too many files on %s";
		if (dqblk.dqb_bhardlimit &&
		    dqblk.dqb_curblocks >= dqblk.dqb_bhardlimit)
			msgb = "Block limit reached on %s";
		else if (enab && dqblk.dqb_bwarn == 0)
			msgb = "Out of block warnings on %s";
		else if (dqblk.dqb_bsoftlimit &&
		    dqblk.dqb_curblocks >= dqblk.dqb_bsoftlimit)
			msgb = "Over disc quota on %s";
		if (dqblk.dqb_iwarn < MAX_IQ_WARN)
			sprintf(iwarn, "%d", dqblk.dqb_iwarn);
		else
			iwarn[0] = '\0';
		if (dqblk.dqb_bwarn < MAX_DQ_WARN)
			sprintf(dwarn, "%d", dqblk.dqb_bwarn);
		else
			dwarn[0] = '\0';
		if (qflag) {
			if (msgi != (char *)0 || msgb != (char *)0)
				ultheading(uid, name);
			if (msgi != (char *)0)
				xprintf(msgi, fs->fs_file);
			if (msgb != (char *)0)
				xprintf(msgb, fs->fs_file);
			continue;
		}
		if (vflag || dqblk.dqb_curblocks || dqblk.dqb_curinodes) {
			ultheading(uid, name);
			printf("%10s%8d%c%7d%8d%8s%8d%c%7d%8d%8s\n"
				, fs->fs_file
				, (dqblk.dqb_curblocks / btodb(1024)) 
				, (msgb == (char *)0) ? ' ' : '*'
				, (dqblk.dqb_bsoftlimit / btodb(1024)) 
				, (dqblk.dqb_bhardlimit / btodb(1024)) 
				, dwarn
				, dqblk.dqb_curinodes
				, (msgi == (char *)0) ? ' ' : '*'
				, dqblk.dqb_isoftlimit
				, dqblk.dqb_ihardlimit
				, iwarn
			);
		}
	}
	endfsent();
	if (!done && !qflag) {
		if (idind)
			putchar('\n');
		xprintf("Disc quotas for %s (uid %d):", name, uid);
		xprintf("none.");
	}
	xprintf(0);
}

ultheading(uid, name)
	int uid;
	char *name;
{

	if (done++)
		return;
	xprintf(0);
	if (qflag) {
		if (!idind)
			return;
		xprintf("User %s (uid %d):", name, uid);
		xprintf(0);
		return;
	}
	putchar('\n');
#if 0
	xprintf("Disc quotas for %s (uid %d):", name, uid);
#endif
	xprintf(0);
	printf("%10s%8s %7s%8s%8s%8s %7s%8s%8s\n"
		, "Filsys"
		, "current"
		, "quota"
		, "limit"
		, "#warns"
		, "files"
		, "quota"
		, "limit"
		, "#warns"
	);
}

xprintf(fmt, arg1, arg2, arg3, arg4, arg5, arg6)
	char *fmt;
{
	char	buf[100];
	static int column;

	if (fmt == 0 && column || column >= 40) {
		putchar('\n');
		column = 0;
	}
	if (fmt == 0)
		return;
	sprintf(buf, fmt, arg1, arg2, arg3, arg4, arg5, arg6);
	if (column != 0 && strlen(buf) < 39)
		while (column++ < 40)
			putchar(' ');
	else if (column) {
		putchar('\n');
		column = 0;
	}
	printf("%s", buf);
	column += strlen(buf);
}
#endif
