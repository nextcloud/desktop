#!/bin/bash
# Last Change: 2008-07-03 11:08:54

for f in $@; do
  test "${f##*/}" = "CMakeLists.txt" && continue
  echo -e "\e[32mCreating asciidoc html document ${f%.*}.html\e[0m"
  asciidoc \
      --attribute=numbered \
      --attribute=icons \
      --attribute="iconsdir=./images/icons" \
      --attribute=toc \
      --backend=xhtml11 \
      --out-file="$(dirname $f)/userguide/${f%.*}.html" \
      $f
  rm -f ${f%.*}.xml
done
