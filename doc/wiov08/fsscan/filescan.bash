#!/bin/bash

# make sure logdir doesn't end up scanned!
LOGDIR=/tmp/scores
BLKSZS="512 1024 2048 4096 8192"

sha1 $1 >> $LOGDIR/full.scores

SIZESTR=`stat -c %s $1`

let size=$SIZESTR
declare -i blksz

for blksz in $BLKSZS; do
	let maxblock=$size/$blksz
	declare -i off=0;
	while [ $off -le $maxblock ]; do
		HASH=`dd if=$1 skip=$off bs=$blksz skip=$off count=1 2>/dev/null| sha1`
		echo $HASH $1 >> $LOGDIR/$blksz.scores
		let off=off+1
	done
done
