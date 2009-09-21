#!/bin/bash

countem() {
	let total=0
	while read count hash; do
		let count=$count-1
		let total=$total+$count
	done

	let bytes=$total*$1
	echo $bytes
}

sort -n -r $1.score | uniq -w 40 -c -d | sort -n -r | head -n 1
