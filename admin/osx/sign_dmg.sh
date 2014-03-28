#!/bin/sh -x

[ "$#" -lt 2 ] && echo "Usage: sign_dmg.sh <dmg> <identity>" && exit

src_dmg="$1"
tmp_dmg="writable_$1"
signed_dmg="signed_$1"
identity="$2"

QT_FMWKS=`basename ${TMP_APP}/Contents/Frameworks`/Qt*
QT_FMWK_VERSION="5"

fix_frameworks() {
    TMP_APP=$1
    QT_FMWK_PATH=$2
    QT_FMWKS=$3/Qt*.framework

    echo "Patching Qt frameworks..."
    for FMWK in $QT_FMWKS; do
        FMWK_NAME=`basename -s .framework $FMWK`
        FMWK=`basename $FMWK`
        FMWK_PATH="${TMP_APP}/Contents/Frameworks/${FMWK}"
        mkdir -p "${FMWK_PATH}/Versions/${QT_FMWK_VERSION}/Resources/"
        cp -avf "${QT_FMWK_PATH}/${FMWK}/Contents/Info.plist" "${FMWK_PATH}/Versions/${QT_FMWK_VERSION}/Resources"
        (cd "${FMWK_PATH}" && ln -sf "Versions/${QT_FMWK_VERSION}/Resources" "Resources")
        perl -pi -e "s/${FMWK_NAME}_debug/${FMWK_NAME}/" "${FMWK_PATH}/Resources/Info.plist"
    done
}

mount="/Volumes/$(basename "$src_dmg"|sed 's,-\([0-9]\)\(.*\),,')"
test -e "$tmp_dmg" && rm -rf "$tmp_dmg"
hdiutil convert "$src_dmg" -format UDRW -o "$tmp_dmg"
hdiutil attach "$tmp_dmg"
pushd "$mount"
fix_frameworks "$mount"/*.app `qmake -query QT_INSTALL_LIBS` "$mount"/*.app/Contents/Frameworks
codesign -s "$identity" --deep "$mount"/*.app
popd
diskutil eject "$mount"
test -e "$signed_dmg" && rm -rf "$signed_dmg"
hdiutil convert "$tmp_dmg" -format UDBZ -o "$signed_dmg"
