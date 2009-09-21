#!/bin/bash

filterem() {
	while read file links size overflow andthen ; do
		if [ -z $overflow ] ; then 
			echo $file $links $size
		fi
	done
}

cat $1 | filterem
