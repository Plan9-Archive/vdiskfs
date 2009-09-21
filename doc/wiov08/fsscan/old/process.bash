#!/bin/bash

wc -l $1 > $1.count
cut -f 1 $1 | sort -n | uniq -c -d | sort -r -n > $1.out
