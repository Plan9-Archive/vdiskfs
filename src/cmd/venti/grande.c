/*
 * Grande - Venti File System Backend
 * Copyright (c) 2009
 */

/* Based on ro.c Copyright (c) 2004 Russ Cox */
#include <u.h>
#include <libc.h>
#include <venti.h>
#include <thread.h>
#include <libsec.h>

#ifndef _UNISTD_H_
#pragma varargck type "F" VtFcall*
#pragma varargck type "T" void
#endif

VtConn *z;
int verbose;
char *stowage;

enum
{
	STACK = 8192,
	MAXNAMELEN = 256,
	STOWDEPTH = 2,
};

void
usage(void)
{
	fprint(2, "usage: venti/grande [-v] [-a address] [-s stowage]\n");
	threadexitsall("usage");
}

int
makedir(char *s, int mode)
{
	int f;

	if(access(s, AEXIST) == 0){
		fprint(2, "mkdir: %s already exists\n", s);
		return -1;
	}
	f = create(s, OREAD, DMDIR | mode);
	if(f < 0){
		fprint(2, "mkdir: can't create %s: %r\n", s);
		return -1;
	}
	close(f);
	return 0;
}

void
mkdirp(char *s, int mode)
{
	char *p;

	for(p=strchr(s+1, '/'); p; p=strchr(p+1, '/')){
		*p = 0;
		if(access(s, AEXIST) != 0 && makedir(s, mode) < 0)
			return;
		*p = '/';
	}
	if(access(s, AEXIST) != 0)
		makedir(s, mode);
}

void
readthread(void *v)
{
	VtReq *r;
	int pathlen = strlen(stowage) + (STOWDEPTH * 3) + VtScoreSize;
	char *filepath = malloc(pathlen);
	uchar *buf;
	int fd, n, sz;
	char err[ERRMAX];
	
	r = v;
	snprint(filepath, pathlen, "%s/%2.2x/%2.2x/%V", stowage, r->tx.score[0], r->tx.score[1], r->tx.score);

	fd = open(filepath, OREAD); 
	if(fd < 0) {
		r->rx.msgtype = VtRerror;
		rerrstr(err, sizeof err);
		r->rx.error = vtstrdup(err);
		goto out;
	}

	sz = r->tx.count;
	buf = malloc(sz);
	n = read(fd, buf, sz);
	if(n < 0) {
		r->rx.msgtype = VtRerror;
		rerrstr(err, sizeof err);
		r->rx.error = vtstrdup(err);
		free(buf);
		goto out;
	}

	r->rx.data = packetforeign(buf, n, free, buf);
	close(fd);	
	
	if(verbose)
		fprint(2, "-> %F\n", &r->rx);
out:
	vtrespond(r);
}

void
writethread(void *v)
{
	VtReq *r;
	int pathlen = strlen(stowage) + (STOWDEPTH * 3) + VtScoreSize;
	char *filepath = malloc(pathlen);
	uchar *buf;
	int fd, sz;

	r = v;

	packetsha1(r->tx.data, r->rx.score);
	snprint(filepath, pathlen, "%s/%2.2x/%2.2x", stowage, r->rx.score[0], r->rx.score[1], r->rx.score);
	mkdirp(filepath, 0777);

	snprint(filepath, pathlen, "%s/%2.2x/%2.2x/%V", stowage, r->rx.score[0], r->rx.score[1], r->rx.score);

	fd = create(filepath, OWRITE, 0444|OEXCL); 
	if(fd < 0) {
		/* File already exists - TODO: Check it */
		goto end;
	}
	sz = packetsize(r->tx.data);
	buf = malloc(sz);
	packetcopy(r->tx.data, buf, 0, sz); 
	write(fd, buf, sz);
	close(fd);
	free(buf);

end:	
	if(verbose)
		fprint(2, "-> %F\n", &r->rx);

	free(filepath);
	vtrespond(r);
}

void
threadmain(int argc, char **argv)
{
	VtReq *r;
	VtSrv *srv;
	char *address;

	fmtinstall('F', vtfcallfmt);
	fmtinstall('V', vtscorefmt);
	
	address = "tcp!*!venti";
	stowage = "/tmp/stow";
	
	ARGBEGIN{
	case 'v':
		verbose++;
		break;
	case 'a':
		address = EARGF(usage());
		break;
	case 's':
		stowage = EARGF(usage());
		break;
	default:
		usage();
	}ARGEND
	
	/* 
	 * check if stowage exists, else create it 
	 */
	mkdirp(stowage, 0777);
	
	srv = vtlisten(address);
	if(srv == nil)
		sysfatal("vtlisten %s: %r", address);

	while((r = vtgetreq(srv)) != nil){
		r->rx.msgtype = r->tx.msgtype+1;
		if(verbose)
			fprint(2, "<- %F\n", &r->tx);
		switch(r->tx.msgtype){
		case VtTping:
			break;
		case VtTgoodbye:
			break;
		case VtTread:
			threadcreate(readthread, r, 16384);
			continue;
		case VtTwrite:
			threadcreate(writethread, r, 16384);
			continue;	
		case VtTsync:
			break;
		}
		if(verbose)
			fprint(2, "-> %F\n", &r->rx);
		vtrespond(r);
	}
	threadexitsall(nil);
}

