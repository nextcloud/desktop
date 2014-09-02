#!/bin/sh -x

[ "$#" -lt 2 ] && echo "Usage: sign_app.sh <app> <identity>" && exit

src_app="$1"
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

fix_frameworks "$src_app" `qmake -query QT_INSTALL_LIBS` "$src_app"/Contents/Frameworks
codesign -s "$identity"  --force --verify --verbose  --deep "$src_app"

# Just for our debug purposes:
spctl -a -t exec -vv $src_app
codesign -dv $src_app