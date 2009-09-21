#!/bin/bash
# new format is imagescan $logdir $file $start $end
# make sure logdir doesn't end up scanned!
LOGDIR=$1
IMAGE=$2
BLKSZS="1024 4096 8192"

mkdir -p $1

# we don't do this for disk images
#sha1 $1 >> $LOGDIR/full.scores

#SIZESTR=`stat -c %s $2`
#let size=$SIZESTR
declare -i START
declare -i END
let START=$3
let END=$4

#echo $1 $2 $3 $4
#echo $START $END

declare -i blksz

for blksz in $BLKSZS; do
	declare -i off=$START/$blksz;
	declare -i end=$END/$blksz
	while [ $off -le $end ]; do
		HASH=`dd if=$IMAGE skip=$off bs=$blksz skip=$off count=1 2>/dev/null| /usr/plan9/bin/sha1sum`
		echo $HASH >> $LOGDIR/$blksz.scores
		let off=off+1
	done
done
