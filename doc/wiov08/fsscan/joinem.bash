#!/bin/bash
join -1 2 -2 1 -o 1.1,2.3,2.2,2.1 file.score file.list | sort -n -k 1,40 > file.join
