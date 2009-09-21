#!/bin/bash
# this gives sorted count of duplicate hashes
sort $1 | uniq -c -w 40 | sort -n -r
