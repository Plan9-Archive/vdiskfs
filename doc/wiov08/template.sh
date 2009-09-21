#!/bin/sh
for i in workingdraft proof webversion finalversion ;
do
  sed "s/XXX/$i/" template-v1.tex > $i-v1.tex
  latex $i-v1.tex
  latex $i-v1.tex
  dvips -G0 -Ppdf -Pcmz -Pamz -o $i-v1.ps $i-v1.dvi
  ps2pdf $i-v1.ps
  rm -f $i-v1.tex $i-v1.dvi $i-v1.log $i-v1.aux $i-v1.ent $i-v1.ps
done
