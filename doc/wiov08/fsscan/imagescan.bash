#!/bin/bash
#
# imagescan $file $start $end $log
#
# make sure logdir doesn't end up scanned!
IMAGE=$1
let START=$2
let END=$3
LOGDIR=$4

BLKSZS="512 1024 2048 4096 8192"

# we don't do this for disk images
#sha1 $IMAGE >> $LOGDIR/full.scores

#SIZESTR=`stat -c %s $2`
#let size=$SIZESTR

declare -i blksz

for blksz in $BLKSZS; do
	let off=$START/$blksz
	let last=$END/$blksz
	while [ $off -le $last ]; do
		echo $IMAGE $off / $last $blksz 
#		HASH=`dd if=$IMAGE skip=$off bs=$blksz skip=$off count=1 2>/dev/null| sha1`
#		echo $HASH $2 >> $LOGDIR/$blksz.scores
		let off=off+1
	done
done
