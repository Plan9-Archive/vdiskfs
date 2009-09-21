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

	while read count hash size links file; do
		let totalfiles=$totalfiles+$count
		if [ $size = 0 ] ; then
			let zerocount=$zerocount+$count
		else
			let links=$links-1
			let totallink=$totallink+$links
			let mag=$links*size
			let totallinksz=$totallinksz+$mag
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

	echo $total
}

compare() {
	echo -n "$1 + $2="
	cat $1/file.join $2/file.join | sort -n -r | uniq -w 40 -c | sort -n -r | countem
}

for firstfile in "$@"; do
	for secondfile in "$@"; do
		if [ $firstfile != $secondfile ]; then
			compare $firstfile $secondfile
		fi
	done
done
