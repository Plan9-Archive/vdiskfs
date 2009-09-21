#!/bin/bash
foo=`/usr/bin/stat -c %s $1`
links=`/usr/bin/stat -c %h $1`
echo $foo $links $1
