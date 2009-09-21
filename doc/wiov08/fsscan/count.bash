#!/bin/bash

let total=0
while read count hash; do
	let total=$total+$count
done

echo $total
