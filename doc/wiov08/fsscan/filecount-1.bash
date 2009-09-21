#!/bin/bash

countem() {
	let zerocount=0
	let total=0
	let totalfiles=0
	let totaldups=0
	let totalsize=0
	let totallink=0
	let totallinksz=0
	declare -i count=0;

	while read -a line; do
		count=${line[0]}
		hash=${line[1]}
		size=${line[2]}
		
		let totalfiles=$totalfiles+$count
		if [ $size = 0 ] ; then
			let zerocount=$zerocount+$count
		else
			let multi=$count
			let mag=$size*$multi
			let totalsize=$totalsize+$mag
			let multi=$multi-1
			let totaldups=$totaldups+$multi
			let mag=$size*$multi
			let total=$total+$mag
			#echo $file $count $links $size $multi $mag $total
		fi
	done

	echo totalfiles-----= $totalfiles
	echo totaldups------= $totaldups
	echo totallinks-----= $totallink
	echo totalsize------= $totalsize
	echo dupsize--------= $total
	echo falsedupsize---= $totallinksz
	echo zerocount------= $zerocount
}

uniq -w 40 -c $1 | sort -n -r | countem
