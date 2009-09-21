#!/bin/bash
find -H -xdev -type f -exec /root/fsscan/sz.bash '{}' \;
