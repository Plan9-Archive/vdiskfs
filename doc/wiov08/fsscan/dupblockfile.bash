#!/bin/bash

countem() {
	let total=0
	while read count hash; do
		#let count=$count-1
		let total=$total+$count
	done

	let bytes=$total*$1
	echo $bytes
}

# same as dupblock but we don't discount zeros
sort -n -r $1.score | uniq -w 40 -c -d | sort -n -r | countem $1
