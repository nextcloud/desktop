#!/bin/bash

useradd user
su - user << EOF
cd /home/user/$1
rm -rf build-win32
mkdir build-win32
cd build-win32
../admin/win/download_runtimes.sh
cmake .. -DCMAKE_TOOLCHAIN_FILE=../admin/win/Toolchain-mingw32-openSUSE.cmake -DWITH_CRASHREPORTER=ON
make -j4
make package

EOF
