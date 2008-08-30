/* Copyright (c) 2008 Eric Van Hensbergen, IBM */
#include <u.h>
#include <libc.h>
#include <thread.h>
#include <sunrpc.h>
#include <nfs3.h>
#include <diskfs.h>

#define debug 0

/* 
 * NOTES:
 * Eventually change prototype for open to allow for an offset based open for
 * partitioned systems.
 *
 * Could do the same thing for block size, but we need to be able to encode it in
 * venti someplace so we don't get mismatches
 *
 */

struct Rawdisk {
	Disk *disk;
	int blocksize;
	Fsys *fsys;
	u64int offset;		/* byte offset (for now..maybe sector later */
};

static Block*
rawblockread(Fsys *fsys, u64int bno)
{
	struct Rawdisk *rd = fsys->priv;

	return diskread(rd->disk, rd->blocksize, rd->offset + (u64int)bno*rd->blocksize);
}

static void
rawclose(Fsys *fsys)
{
	struct Rawdisk *rd;

	rd = fsys->priv;
	free(rd);
	free(fsys);	
}

Fsys*
fsysopenraw(Disk *disk)
{
	Fsys *fsys;
	struct Rawdisk *rd;

	fsys = emalloc(sizeof(Fsys));
	rd = emalloc(sizeof(struct Rawdisk));
	rd->disk = disk;
	rd->blocksize = 4096;	/* hardcode for now */
	rd->offset = 0;			/* hardcode for now */
	rd->fsys = fsys;
	fsys->blocksize = rd->blocksize;
	fsys->nblock = disk->_size(disk)/fsys->blocksize;

	print("nblock: %d %d\n", disk->_size(disk), fsys->nblock);

	fsys->priv = rd;
	fsys->type = "rawdisk";
	fsys->_readblock = rawblockread;
	fsys->_close = rawclose;

	return fsys;
}
