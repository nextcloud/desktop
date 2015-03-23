#!/bin/sh -x

#VS2013
base_url=http://download.microsoft.com/download/2/E/6/2E61CFA4-993B-4DD4-91DA-3737CD5CD6E3
tmp_path=${1:-/tmp/.vcredist}

mkdir -p $tmp_path

copy_cached_file() {
  file=$1
  if [ ! -e $tmp_path/$file ]; then
    wget -O $tmp_path/$file $base_url/$file
  fi
  cp -a $tmp_path/$file $PWD
}

copy_cached_file "vcredist_x64.exe"
copy_cached_file "vcredist_x86.exe"

