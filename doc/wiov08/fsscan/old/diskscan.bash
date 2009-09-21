#!/bin/bash
#
#parted -s -m /mnt/images/kvm/test-fedora-9-x86_64.img unit b print
#
# skip first couple of lines of output
#
# for each remaining line
# parse out start, end, and types
# if ext2/ext3 then scan it
#

LOGDIR=$1
IMAGE=$2 

parse_lines() {
	read shit;
	read summary;
	export IFS=$IFS:':'
	while read partnum start end size type foo bar; do
		if [ $type = linux-swap ]; then
			echo skipping swap
		else
			start=`echo $start | sed s/B//g`
			end=`echo $end | sed s/B//g`
			echo $partnum $start $end $type
			/root/fsscan/imagescan.bash $LOGDIR/$partnum $IMAGE $start $end 
		fi
	done;
}

cat $LOGDIR/part-table | parse_lines
