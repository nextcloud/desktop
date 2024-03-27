#! /bin/bash

set -xe

export APPNAME=${APPNAME:-nextcloud}
export BUILD_UPDATER=${BUILD_UPDATER:-OFF}
export BUILDNR=${BUILDNR:-0000}
export DESKTOP_CLIENT_ROOT=${DESKTOP_CLIENT_ROOT:-/home/user}

#Set Qt-5.15
export QT_BASE_DIR=/opt/kdeqt5.15

export QTDIR=$QT_BASE_DIR
export PATH=$QT_BASE_DIR/bin:$PATH

# Set defaults
export SUFFIX=${DRONE_PULL_REQUEST:=master}
if [ $SUFFIX != "master" ]; then
    SUFFIX="PR-$SUFFIX"
fi
if [ "$BUILD_UPDATER" != "OFF" ]; then
    BUILD_UPDATER=ON
fi

mkdir /app

# Build client
mkdir build-client
cd build-client
cmake \
    -G Ninja \
    -D CMAKE_INSTALL_PREFIX=/usr \
    -D BUILD_TESTING=OFF \
    -D BUILD_UPDATER=$BUILD_UPDATER \
    -D MIRALL_VERSION_BUILD=$BUILDNR \
    -D MIRALL_VERSION_SUFFIX="$VERSION_SUFFIX" \
    -D CMAKE_UNITY_BUILD=ON \
    ${DESKTOP_CLIENT_ROOT}
cmake --build . --target all
DESTDIR=/app cmake --install .

# Move stuff around
cd /app

mv usr/lib/x86_64-linux-gnu/* usr/lib/

mkdir usr/plugins
mv usr/lib/*sync_vfs_suffix.so usr/plugins
mv usr/lib/*sync_vfs_xattr.so usr/plugins

rm -rf usr/lib/cmake
rm -rf usr/include
rm -rf usr/mkspecs
rm -rf usr/lib/x86_64-linux-gnu/

# Don't bundle the explorer extensions as we can't do anything with them in the AppImage
rm -rf usr/share/caja-python/
rm -rf usr/share/nautilus-python/
rm -rf usr/share/nemo-python/

# Move sync exclude to right location
mv /app/etc/*/sync-exclude.lst usr/bin/
rm -rf etc

# com.nextcloud.desktopclient.nextcloud.desktop
DESKTOP_FILE=$(ls /app/usr/share/applications/*.desktop)
sed -i -e 's|Icon=nextcloud|Icon=Nextcloud|g' ${DESKTOP_FILE} # Bug in desktop file?
cp ./usr/share/icons/hicolor/512x512/apps/*.png . # Workaround for linuxeployqt bug, FIXME

# Because distros need to get their shit together
cp -R /usr/lib/x86_64-linux-gnu/libssl.so* ./usr/lib/
cp -R /usr/lib/x86_64-linux-gnu/libcrypto.so* ./usr/lib/
cp -P /usr/local/lib*/libssl.so* ./usr/lib/
cp -P /usr/local/lib*/libcrypto.so* ./usr/lib/
cp -P /usr/local/lib*/libsqlite*.so* ./usr/lib/

# NSS fun
cp -P -r /usr/lib/x86_64-linux-gnu/nss ./usr/lib/

# Use linuxdeployqt to deploy
LINUXDEPLOYQT_VERSION="continuous"
wget -O linuxdeployqt.AppImage --ca-directory=/etc/ssl/certs -c "https://github.com/probonopd/linuxdeployqt/releases/download/${LINUXDEPLOYQT_VERSION}/linuxdeployqt-continuous-x86_64.AppImage"
chmod a+x linuxdeployqt.AppImage
./linuxdeployqt.AppImage --appimage-extract
rm ./linuxdeployqt.AppImage
cp -r ./squashfs-root ./linuxdeployqt-squashfs-root
unset QTDIR; unset QT_PLUGIN_PATH ; unset LD_LIBRARY_PATH
export LD_LIBRARY_PATH=/usr/local/lib:/usr/local/lib/x86_64-linux-gnu
./squashfs-root/AppRun ${DESKTOP_FILE} -bundle-non-qt-libs -qmldir=${DESKTOP_CLIENT_ROOT}/src/gui

# Set origin
./squashfs-root/usr/bin/patchelf --set-rpath '$ORIGIN/' /app/usr/lib/lib*sync.so.0

# Build AppImage
./squashfs-root/AppRun ${DESKTOP_FILE} -appimage -updateinformation="gh-releases-zsync|nextcloud-releases|desktop|latest|Nextcloud-*-x86_64.AppImage.zsync"

# Workaround issue #103
rm -rf ./squashfs-root
APPIMAGE=$(ls *.AppImage)
"./${APPIMAGE}" --appimage-extract
rm "./${APPIMAGE}"
rm ./squashfs-root/usr/lib/libglib-2.0.so.0
rm ./squashfs-root/usr/lib/libgobject-2.0.so.0
PATH=./linuxdeployqt-squashfs-root/usr/bin:$PATH appimagetool -n ./squashfs-root "$APPIMAGE"

#move AppImage
if [ ! -z "$DRONE_COMMIT" ]
then
    mv *.AppImage ${APPNAME}-${SUFFIX}-${DRONE_COMMIT}-x86_64.AppImage
fi
mv *.AppImage ${DESKTOP_CLIENT_ROOT}/
