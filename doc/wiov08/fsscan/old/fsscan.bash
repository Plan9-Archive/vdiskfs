#!/bin/bash

#
# for all files
#	for each blocksize (512, 1024, 2048, 4096, 8192, full file)
#		dd file blocksize | sha1 > central.score.blocksize.index
#		
#
#

export LOGDIR=/tmp/$1
mkdir -p $LOGDIR
/root/fsscan/filesz.bash > $LOGDIR/file.sizes &
/usr/bin/find -H -xdev -type f -exec /root/fsscan/filescan.bash '{}' \;
