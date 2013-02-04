#!/bin/bash
# Last Change: 2008-07-03 11:08:54

if [ "x${MAN_DIR}" == 'x' ]; then
  MAN_DIR=`pwd`
fi

mkdir -p ${MAN_DIR}

for f in $@; do
  test "${f##*/}" = "CMakeLists.txt" && continue
  echo -e "\e[32mCreating manpage ${f%.*}\e[0m"
  a2x --doctype=manpage --format=manpage $f --destination-dir ${MAN_DIR}
  rm -f ${f%.*}.xml
done
