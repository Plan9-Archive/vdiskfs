
/* Copyright (c) 2008 - Eric Van Hensbergem IBM */

#include <u.h>
#include <libc.h>
#include <venti.h>
#include <diskfs.h>
#include <thread.h>
#include <fcall.h>
#include <sys/types.h>
#include <dirent.h>

/* TODO: Necessary? */
enum
{
	OPERM	= 0x3,		/* mask of all permission types in open mode */
	Maxfdata	= 8192
};

typedef struct Fid Fid;
typedef struct PosixFile PosixFile;
typedef struct Vdisk Vdisk;

struct VentiState {
	VtConn 		*z;
	VtCache 		*vc;
};

struct Vdisk {
	int 			active;
	int 			blocksize;
	unsigned char 	score[VtScoreSize];
	Fsys 			*fsys;
	Disk 			*disk;
};

struct PosixFile {

	int				fd;
	DIR				*dir;		/* ugh - fuck */
	int				diroffset;	/* ugh - fuck */
	char 			*direntname;/* ugh? fuck? */
	struct stat		stat;
	Qid				qid;	
};

struct Fid
{
	short			busy;
	short			open;
	short			rclose;
	int				fid;
	Fid				*next;

	char			*path;
	PosixFile		file;
	Vdisk 			vdisk;
};

/* TODO: necessary? */
enum
{
	Pexec =		1,
	Pwrite = 	2,
	Pread = 	4,
	Pother = 	1,
	Pgroup = 	8,
	Powner =	64
};

char	*rflush(Fid*), *rversion(Fid*), *rauth(Fid*),
		*rattach(Fid*), *rwalk(Fid*),
		*ropen(Fid*), *rcreate(Fid*),
		*rread(Fid*), *rwrite(Fid*), *rclunk(Fid*),
		*rremove(Fid*), *rstat(Fid*), *rwstat(Fid*);

char 	*(*fcalls[Tmax])(Fid*);

char	Eperm[] =		"permission denied";
char	Enotdir[] =		"not a directory";
char	Enoauth[] =		"vdiskfs: authentication not required";
char	Enotexist[] =	"file does not exist";
char	Einuse[] =		"file in use";
char	Eexist[] =		"file exists";
char	Eisdir[] =		"file is a directory";
char	Enotowner[] =	"not owner";
char	Eisopen[] = 	"file already open for I/O";
char	Excl[] = 		"exclusive use file already open";
char	Ename[] = 		"illegal name";
char	Eversion[] =	"unknown 9P version";
char	Enotempty[] =	"directory not empty";
char	Ebadfid[] =		"bad fid";

char *rootpath = "/";	/* top of the exported tree */

/* BUG: wtf is with all the globals
 * is it not possible to do multithreaded server using the
 * standard p9p fsys interfaces?
 */

struct VentiState ventistate;
int dotu = 0; 		
Fid	*fids;
int debug;
int private;
Fcall thdr;
Fcall rhdr;
uchar	mdata[IOHDRSZ+Maxfdata];
char	rdata[Maxfdata];	/* buffer for data in reply */
int	messagesize = sizeof mdata;
int	mfd[2];

void
notifyf(void *a, char *s)
{
	USED(a);
	if(strncmp(s, "interrupt", 9) == 0)
		noted(NCONT);
	noted(NDFLT);
}

void
error(char *s)
{
	fprint(2, "%s: %s: %r\n", argv0, s);
	threadexits(s);
}

void *
emallocz(ulong n)
{
	void *p;

	p = mallocz(n, 1);
	if(!p)
		error("out of memory");

	return p;
}

void *
erealloc(void *p, ulong n)
{
	p = realloc(p, n);
	if(!p)
		error("out of memory");
	return p;
}

char *
estrdup(char *q)
{
	char *p;
	int n;

	n = strlen(q)+1;
	p = malloc(n);
	if(!p)
		error("out of memory");
	memmove(p, q, n);
	return p;
}

Fid *
newfid(int fid)
{
	Fid *f, *ff;

	ff = 0;
	for(f = fids; f; f = f->next)
		if(f->fid == fid)
			return f;
		else if(!ff && !f->busy)
			ff = f;
	if(ff){
		ff->fid = fid;
		return ff;
	}
	f = emallocz(sizeof *f);
	f->fid = fid;
	f->next = fids;
	fids = f;
	return f;
}

static uchar
ustat2qidtype(struct stat *st)
{
	uchar ret;

	ret = 0;
	if (S_ISDIR(st->st_mode))
		ret |= QTDIR;

	if (S_ISLNK(st->st_mode))
		ret |= QTSYMLINK;

	return ret;
}

static void
ustat2qid(struct stat *st, Qid *qid)
{
	int n;

	qid->path = 0;
	n = sizeof(qid->path);
	if (n > sizeof(st->st_ino))
		n = sizeof(st->st_ino);
	memmove(&qid->path, &st->st_ino, n);
	qid->vers = st->st_mtime ^ (st->st_size << 8);
	qid->type = ustat2qidtype(st);
}

static uint
umode2npmode(mode_t umode, int dotu)
{
	uint ret;

	ret = umode & 0777;
	if (S_ISDIR(umode))
		ret |= DMDIR;

	if (dotu) {
		if (S_ISLNK(umode))
			ret |= DMSYMLINK;
		if (S_ISSOCK(umode))
			ret |= DMSOCKET;
		if (S_ISFIFO(umode))
			ret |= DMNAMEDPIPE;
		if (S_ISBLK(umode))
			ret |= DMDEVICE;
		if (S_ISCHR(umode))
			ret |= DMDEVICE;
		if (umode & S_ISUID)
			ret |= DMSETUID;
		if (umode & S_ISGID)
			ret |= DMSETGID;
	}

	return ret;
}

static int
ustat2dir(char *path, struct stat *st, uchar *statbuf, int nbuf, int dotu)
{
	int err;
	Dir wstat;
	char uname[255];
	char gname[255];
	char ext[255];
	char *s;
	int n;

	memset(&wstat, 0, sizeof(Dir));
	ustat2qid(st, &wstat.qid);
	wstat.mode = umode2npmode(st->st_mode, dotu);
	wstat.atime = st->st_atime;
	wstat.mtime = st->st_mtime;
	wstat.length = st->st_size;

	sprint(uname, "%d", st->st_uid);
	sprint(gname, "%d", st->st_gid);
	wstat.uid = uname;
	wstat.gid = uname;
	wstat.muid = "";

	wstat.ext = NULL;
	if (dotu) {
		wstat.uidnum = st->st_uid;
		wstat.gidnum = st->st_gid;

		if (wstat.mode & DMSYMLINK) {
			err = readlink(path, ext, sizeof(ext) - 1);
			if (err < 0)
				err = 0;

			ext[err] = '\0';
		} else if (wstat.mode & DMDEVICE) {
			snprint(ext, sizeof(ext), "%c %u %u", 
				S_ISCHR(st->st_mode)?'c':'b',
				major(st->st_rdev), minor(st->st_rdev));
		} else {
			ext[0] = '\0';
		}

		wstat.ext = ext;
	}

	s = strrchr(path, '/');
	if (s)
		wstat.name = s + 1;
	else
		wstat.name = path;

	n = convD2Mu(&wstat, statbuf, nbuf, dotu);
	if(n > 2)
		return n;

	return 0;	
}

static int
omode2uflags(uchar mode)
{
	int ret;

	ret = 0;
	switch (mode & 3) {
	case OREAD:
		ret = O_RDONLY;
		break;

	case ORDWR:
		ret = O_RDWR;
		break;

	case OWRITE:
		ret = O_WRONLY;
		break;

	case OEXEC:
		ret = O_RDONLY;
		break;
	}

	if (mode & OTRUNC)
		ret |= O_TRUNC;

	if (mode & OAPPEND)
		ret |= O_APPEND;

	if (mode & OEXCL)
		ret |= O_EXCL;

	return ret;
}

char*
rversion(Fid *x)
{
	Fid *f;

	USED(x);
	for(f = fids; f; f = f->next)
		if(f->busy)
			rclunk(f);
	if(thdr.msize > sizeof mdata)
		rhdr.msize = sizeof mdata;
	else
		rhdr.msize = thdr.msize;
	messagesize = rhdr.msize;
	if(strncmp(thdr.version, "9P2000", 6) != 0)
		return Eversion;

	rhdr.version = "9P2000";
	return 0;
}

char*
rauth(Fid *x)
{
	if(x->busy)
		return Ebadfid;
	return "vdiskfs: no authentication required";
}

/* BUG: This is likely unfortunate and will cause problems */
char*
rflush(Fid *f)
{
	USED(f);
	return 0;
}

char*
rattach(Fid *f)
{
	char *err = nil;

	/* no authentication! */
	if(f->busy)
		return Ebadfid;
	f->busy = 1;
	f->rclose = 0;
	
	f->path = estrdup(rootpath);
	if(strlen(thdr.aname)) {
		f->path = emallocz( strlen(rootpath) + 1 + strlen(thdr.aname) );
		strcpy(f->path, rootpath);
		strcat(f->path, "/");
		strcat(f->path, thdr.aname);
	} else
		f->path = estrdup(rootpath);
		
	if (lstat(f->path, &f->file.stat) < 0) {
		free(f->path);
		err = Enotexist;
	} else {
		ustat2qid(&f->file.stat, &f->file.qid);
		memcpy(&rhdr.qid, &f->file.qid, sizeof(Qid));
	}

	return err;
}

char*
xclone(Fid *f, Fid **nf)
{
	if(!f->busy)
		return Ebadfid;
	if(f->open)
		return Eisopen;

	*nf = newfid(thdr.newfid);
	(*nf)->busy = 1;
	(*nf)->open = 0;
	(*nf)->rclose = 0;
	(*nf)->path = estrdup(f->path);

	/* TODO: we may not need to do this because only open files
		will have these sections and we can't clone open files */
	memcpy(&(*nf)->file, &f->file, sizeof(PosixFile));
	memcpy(&(*nf)->vdisk, &f->vdisk, sizeof(Vdisk));

	return 0;
}

char*
rwalk(Fid *f)
{
	Fid *nf;
	char *err;
	int i, n;
	int pathlen;
	char *path;

	if(!f->busy)
		return Ebadfid;
	err = nil;
	nf = nil;
	rhdr.nwqid = 0;
	if(thdr.newfid != thdr.fid){
		err = xclone(f, &nf);
		if(err)
			return err;
		f = nf;	/* walk the new fid */
	}

	if(thdr.nwname > 0){
		n = strlen(f->path);
		pathlen = n + 1;
		for(i=0; i<thdr.nwname && i<MAXWELEM; i++)
			pathlen += strlen(thdr.wname[i]) + 1;

		path = emallocz(pathlen);
		memcpy(path, f->path, n);

		for(i=0; i<thdr.nwname && i<MAXWELEM; i++) {
			int len = strlen(thdr.wname[i]);
			path[n++] = '/';
			memcpy(path + n, thdr.wname[i], len);
			n += len;
			path[n] = '\0';
			if (debug)
				fprint(2, "f %d walk %s %s\n", f->fid, f->path, path);
			if (lstat(path, &f->file.stat) < 0) {
				free(path);
				err = Enotexist;
				break;
			} else
				ustat2qid(&f->file.stat, &rhdr.wqid[rhdr.nwqid++]);
		}
	
		if(err == nil) {
			/* success */
			free(f->path);
			f->path = path;
			ustat2qid(&f->file.stat, &f->file.qid);
		} else {
			if(nf!=nil)
				rclunk(nf);
			if(rhdr.nwqid==0)
				return err;
		}
	}

	return err;
}

char *
ropen(Fid *f)
{
	int mode;

	if(!f->busy)
		return Ebadfid;
	if(f->open)
		return Eisopen;

	mode = thdr.mode;

	if(f->file.qid.type & QTDIR){
		if(mode != OREAD)
			return Eperm;
		rhdr.qid = f->file.qid;
		return 0;
	}

	f->file.fd = open(f->path, omode2uflags(mode));
	if (f->file.fd < 0)
		return Eperm;
fprint(2, "open setup fd %d mode: %x %x\n",f->file.fd, thdr.mode, omode2uflags(mode));

	rhdr.qid = f->file.qid; 
	rhdr.iounit = messagesize-IOHDRSZ;
	f->open = 1;
	return 0;
}

char *
rcreate(Fid *f)
{
	/* TODO: vdiskfs is readonly right now */

	return Eperm;
}

#ifdef LATER

char*
rread_dir(Fid *f, char *buf, long offset, int size)
{
	int n, plen;
	struct dirent *dirent;
	char *dname;

	/* TODO: eventually we probably want readdir to work */

	if (offset = 0) {
		rewinddir(f->file.dir);
		f->file.diroffset = 0;
	}

	plen = strlen(f->path);
	n = 0;
	dirent = NULL;
	dname = f->file.direntname;
        while (n < count) {
                if (!dname) {
                        dirent = readdir(f->file.dir);
                        if (!dirent)
                                break;

                        if (strcmp(dirent->d_name, ".") == 0
                        || strcmp(dirent->d_name, "..") == 0)
                                continue;

                        dname = dirent->d_name;
                }

                path = malloc(plen + strlen(dname) + 2);
                sprintf(path, "%s/%s", f->path, dname);

                if (lstat(path, &st) < 0) {
                        free(path);
                        create_rerror(errno);
                        return 0;
                }
		
		rhdr.nstat = ustat2dir(f->path, &f->file.stat, statbuf, 
								STATMAX, dotu);

	/* TODO: extensions */
                free(path);
                path = NULL;
                if (i==0)
                        break;

                dname = NULL;
                n += i;
        }
        if (f->file.direntname) {
                free(f->file.direntname);
                f->file.direntname = NULL;
        }

        if (dirent)
                f->file.direntname = strdup(dirent->d_name);

        f->file.diroffset += n;

	/* TODO: This is weird... */
        return n;
}

#endif

char*
rread(Fid *f)
{
	char *buf;
	long off;
	int n, cnt;

	if(!f->busy)
		return Ebadfid;

	n = 0;
	rhdr.count = 0;
	off = thdr.offset;
	buf = rdata;
	cnt = thdr.count;

	if(cnt > messagesize)	/* shouldn't happen, anyway */
		cnt = messagesize;

	if(f->file.qid.type & QTDIR)
		return Eperm; /* return rread_dir(f, buf, off, cnt); */

	n = pread(f->file.fd, buf, cnt, off);
	if (n < 0) {
		fprint(2, "pread returned %d\n",n);
		return strerror(n);
	}
	
	rhdr.data = buf;
	rhdr.count = n;
	return 0;
}

char*
rwrite(Fid *f)
{
	/* TODO: vdiskfs is readonly right now */

	return Eperm;
}

char *
rclunk(Fid *f)
{
	char *e = nil;

	if (f->open)
		close(f->file.fd);

	f->busy = 0;
	f->open = 0;

	free(f->path);
	f->path = nil;

	memset(&f->vdisk, 0, sizeof(Vdisk));
	memset(&f->file, 0, sizeof(PosixFile));

	return e;
}

char *
rremove(Fid *f)
{
	/* TODO: vdiskfs is readonly right now */

	return Eperm;
}

char *
rstat(Fid *f)
{	
	/* TODO: This sucks - need better cleanup */
	static uchar statbuf[STATMAX]; 

	if(!f->busy)
		return Ebadfid;

fprint(2, "Stat Path: %s\n", f->path);

	if (lstat(f->path, &f->file.stat) < 0)
		return Enotexist;


	rhdr.nstat = ustat2dir(f->path, &f->file.stat, statbuf, STATMAX, dotu);
	rhdr.stat = statbuf;
	return 0;
}

char *
rwstat(Fid *f)
{
	/* TODO: vdiskfs is readonly right now */

	return Eperm;
}


void
io(void)
{
	char *err, buf[20];
	int n, pid, ctl;

	pid = getpid();
	if(private){
		snprint(buf, sizeof buf, "/proc/%d/ctl", pid);
		ctl = open(buf, OWRITE);
		if(ctl < 0){
			fprint(2, "can't protect vdiskfs\n");
		}else{
			fprint(ctl, "noswap\n");
			fprint(ctl, "private\n");
			close(ctl);
		}
	}

	for(;;){
		/*
		 * reading from a pipe or a network device
		 * will give an error after a few eof reads.
		 * however, we cannot tell the difference
		 * between a zero-length read and an interrupt
		 * on the processes writing to us,
		 * so we wait for the error.
		 */
		n = read9pmsg(mfd[0], mdata, messagesize);
		if(n < 0)
			error("mount read");
		if(n == 0)
			error("mount eof");
		if(convM2Su(mdata, n, &thdr, dotu) == 0)
			continue;

		if(debug)
			fprint(2, "vdiskfs %d:<-%F\n", pid, &thdr);

		if(!fcalls[thdr.type])
			err = "bad fcall type";
		else
			err = (*fcalls[thdr.type])(newfid(thdr.fid));

		if(err){
			/* TODO: see if we can work in a numeric id here to keep anthony happy */
			rhdr.type = Rerror;
			rhdr.ename = err;
			if(debug)
				fprint(2, "vdiskfs %d: error: %s\n", pid, err);
		}else{
			rhdr.type = thdr.type + 1;
			rhdr.fid = thdr.fid;
		}
		rhdr.tag = thdr.tag;
		if(debug)
			fprint(2, "vdiskfs %d:->%F\n", pid, &rhdr);/**/
		n = convS2Mu(&rhdr, mdata, messagesize, dotu);
		if(n == 0)
			error("convS2M error on write");
		if(write(mfd[1], mdata, n) != n)
			error("mount write");
	}
}

void
usage(void)
{
	fprint(2, "usage: %s [-is] [-m mountpoint]\n", argv0);
	threadexits("usage");
}

static void
initfcalls(void)
{
	fcalls[Tversion]=	rversion;
	fcalls[Tflush]=		rflush;
	fcalls[Tauth]=		rauth;
	fcalls[Tattach]=	rattach;
	fcalls[Twalk]=		rwalk;
	fcalls[Topen]=		ropen;
	fcalls[Tcreate]=	rcreate;
	fcalls[Tread]=		rread;
	fcalls[Twrite]=		rwrite;
	fcalls[Tclunk]=		rclunk;
	fcalls[Tremove]=	rremove;
	fcalls[Tstat]=		rstat;
	fcalls[Twstat]=		rwstat;
}

static int
initventi(void)
{
	if((ventistate.z = vtdial(nil)) == nil) {
			fprint(2, "vtdial: %r");
			return -1;
	}

	if(vtconnect(ventistate.z) < 0) {
		fprint(2, "vtconnect: %r");
		return -1;
	}

	/* TODO: parameterize cache */
	if((ventistate.vc = vtcachealloc(ventistate.z, 16384, 32)) == nil) {
		fprint(2, "vtcache: %r");
		return -1;
	}

	return 0;
}

void
threadmain(int argc, char *argv[])
{
	char *defmnt;
	int p[2];
	int stdio = 0;
	char *service;

	initfcalls();
	service = "vdiskfs";
	defmnt = nil;
	ARGBEGIN{
	case 'D':
		debug = 1;
		break;
	case 'i':
		defmnt = 0;
		stdio = 1;
		mfd[0] = 0;
		mfd[1] = 1;
		break;
	case 's':
		defmnt = nil;
		break;
	case 'm':
		defmnt = ARGF();
		break;
	case 'r':
		rootpath = ARGF();
		break;
	case 'p':
		private++;
		break;
	case 'S':
		defmnt = 0;
		service = EARGF(usage());
		break;
	default:
		usage();
	}ARGEND

	if(initventi() < 0)
		error("venti connection failed");

	if(pipe(p) < 0)
		error("pipe failed");
	if(!stdio){
		mfd[0] = p[0];
		mfd[1] = p[0];
		if(post9pservice(p[1], service, nil) < 0)
			sysfatal("post9pservice %s: %r", service);
	}

	notify(notifyf);

	if(debug)
		fmtinstall('F', fcallfmt);

	switch(rfork(RFFDG|RFPROC|RFNAMEG|RFNOTEG)){
	case -1:
		error("fork");
	case 0:
		close(p[1]);
		io();
		break;
	default:
		close(p[0]);	/* don't deadlock if child fails */
	}
	threadexits(0);
}

