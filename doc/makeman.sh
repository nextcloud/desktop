#!/bin/bash
# Last Change: 2008-07-03 11:08:54

for f in $@; do
  test "${f##*/}" = "CMakeLists.txt" && continue
  a2x --doctype=manpage --format=manpage $f
  rm ${f%.*}.xml
done
