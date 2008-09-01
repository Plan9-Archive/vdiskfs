#include <u.h>
#include <libc.h>
#include <venti.h>
#include <diskfs.h>
#include <thread.h>

void
usage(void)
{
	fprint(2, "usage: vcat [-z] score >diskfile\n");
	threadexitsall("usage");
}

void
threadmain(int argc, char **argv)
{
	char *pref;
	int zerotoo;
	uchar score[VtScoreSize], prev[VtScoreSize];
	u8int *zero;
	u32int i;
	u32int n;
	int verbose = 0;
	Disk *disk;
	Fsys *fsys;
	VtCache *c;
	VtConn *z;
	VtBlock *b;
	VtEntry e;
	VtRoot root;
	VtFile*	vfile;			/* venti file being written */

	zerotoo = 0;

	ARGBEGIN{
	case 'z':
		zerotoo = 1;
		break;
	default:
		usage();
	}ARGEND

	if(argc != 1)
		usage();

	fmtinstall('V', vtscorefmt);

	memset(prev, 0, sizeof prev);

	if(vtparsescore(argv[0], &pref, score) < 0)
		sysfatal("bad score '%s'", argv[0]);
	if((z = vtdial(nil)) == nil)
		sysfatal("vtdial: %r");
	if(vtconnect(z) < 0)
		sysfatal("vtconnect: %r");
	if((c = vtcachealloc(z, 16384, 32)) == nil)
		sysfatal("vtcache: %r");
	if((disk = diskopenventi(c, score)) == nil)
		sysfatal("diskopenventi: %r");
	if((fsys = fsysopen(disk)) == nil)
		sysfatal("fsysopen: %r");

	b = vtcacheglobal(c, score, VtRootType);
	if(b){
		if(vtrootunpack(&root, b->data) < 0)
			sysfatal("bad root: %r");
		if(strcmp(root.type, fsys->type) != 0)
			sysfatal("root is %s but fsys is %s", root.type, fsys->type);
		memmove(prev, score, VtScoreSize);
		memmove(score, root.score, VtScoreSize);
		vtblockput(b);
	}
	b = vtcacheglobal(c, score, VtDirType);
	if(b == nil)
		sysfatal("vtcacheglobal %V: %r", score);
	if(vtentryunpack(&e, b->data, 0) < 0)
		sysfatal("%V: vtentryunpack failed", score);
	if(verbose)
		fprint(2, "entry: size %llud psize %d dsize %d\n",
			e.size, e.psize, e.dsize);
	vtblockput(b);
	if((vfile = vtfileopenroot(c, &e)) == nil)
		sysfatal("vtfileopenroot: %r");
	vtfilelock(vfile, VtORDWR);

	zero = emalloc(fsys->blocksize);
	fprint(2, "%d blocks total\n", fsys->nblock);
	n = 0;
	for(i=0; i<fsys->nblock; i++){
		if(vtfileblockscore(vfile, i, score) < 0)
			sysfatal("vtfileblockhash %d: %r", i);
		print("%V\n", score);
	}
	threadexitsall(nil);
}
