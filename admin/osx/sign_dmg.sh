#!/bin/sh -x

[ "$#" -lt 2 ] && echo "Usage: sign_dmg.sh <dmg> <identity>" && exit

src_dmg=$1
tmp_dmg=writable_$1
signed_dmg=signed_$1
identity=$2
mount="/Volumes/$(basename $src_dmg|cut -d"-" -f1)"

test -e $tmp_dmg && rm -rf $tmp_dmg
hdiutil convert $src_dmg -format UDRW -o $tmp_dmg
hdiutil attach $tmp_dmg
pushd $mount
codesign -s "$identity" $mount/*.app
popd
diskutil eject $mount
test -e $signed_dmg && rm -rf $signed_dmg
hdiutil convert $tmp_dmg -format UDBZ -o $signed_dmg
