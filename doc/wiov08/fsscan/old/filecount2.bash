#!/bin/bash

countem() {
	let zerocount=0
	let total=0
	let totalfiles=0
	let totaldups=0
	declare -i count=0;

	while read count hash size links file; do
		let totalfiles=$totalfiles+$count
		if [ $size = 0 ] ; then
			let zerocount=$zerocount+$count
		else
			let multi=$count-$links+1
			let totaldups=$totaldups+$multi
			let mag=$size*$multi
			let total=$total+$mag
			#echo $file $count $links $size $multi $mag $total
		fi
	done

	echo totalfiles 	totaldups  	dupsize		zerocount 
	echo $totalfiles	$totaldups	$total 		$zerocount
}

uniq -w 40 -d -c $1 | sort -n -r | countem
