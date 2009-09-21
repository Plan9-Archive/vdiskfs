#!/bin/bash
join -1 3 -2 3 -o 1.1,2.1,2.2,1.3 file.score file.list2 | sort -n -k 1,40 > file.join
