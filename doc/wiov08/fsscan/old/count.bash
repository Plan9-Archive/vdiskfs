#!/bin/bash

let total=0
declare -i count=0;

while read count; do
	let total=$total+$count
done

echo $total
