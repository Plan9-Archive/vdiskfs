/* Copyright (c) 2008 Eric Van Hensbergen, IBM */
#include <u.h>
#include <libc.h>
#include <thread.h>
#include <sunrpc.h>
#include <nfs3.h>
#include <diskfs.h>

#define debug 0

/* 
 * Settable offset is great and all, but we'll need some way of adjusting
 * on read, no? 
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
fsysopenraw(Disk *disk, u32int blocksize, u64int offset)
{
	Fsys *fsys;
	struct Rawdisk *rd;

	fsys = emalloc(sizeof(Fsys));
	rd = emalloc(sizeof(struct Rawdisk));
	rd->disk = disk;
	rd->blocksize = blocksize;
	rd->offset = offset;
	rd->fsys = fsys;
	fsys->blocksize = rd->blocksize;
	fsys->nblock = disk->_size(disk)/fsys->blocksize;

	fsys->priv = rd;
	fsys->type = "rawdisk";
	fsys->_readblock = rawblockread;
	fsys->_close = rawclose;

	return fsys;
}
