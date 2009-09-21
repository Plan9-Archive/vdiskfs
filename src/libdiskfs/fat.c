#include <u.h>
#include <libc.h>
#include <diskfs.h>

Fsys*
fsysopenfat(Disk *disk, u32int bs, u64int off)
{
	USED(disk);
	return nil;
}

