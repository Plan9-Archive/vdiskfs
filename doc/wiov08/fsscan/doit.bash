#!/bin/bash
rm /tmp/*.score
find -H -xdev -type f | /usr/plan9/bin/fsscan > /tmp/file.score
/home/ericvh/src/fsscan/filelist.bash > /tmp/file.list
