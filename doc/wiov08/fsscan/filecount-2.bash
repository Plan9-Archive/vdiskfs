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

	while read size links file; do
		let totalfiles=$totalfiles+1
		if [ $size = 0 ] ; then
			let zerocount=$zerocount+$count
		else
			let links=$links-1
			let totallink=$totallink+$links
			let mag=$links*size
			let totallinksz=$totallinksz+$mag
			let multi=1
			let mag=$size*$multi
			let totalsize=$totalsize+$mag
			let multi=$multi-1
			let totaldups=$totaldups+$multi
			let mag=$size*$multi
			let total=$total+$mag
			#echo $file $count $links $size 
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

sort -n -r $1 | countem
