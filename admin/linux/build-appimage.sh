#! /bin/bash

set -xe

export APPNAME=${APPNAME:-nextcloud}
export BUILD_UPDATER=${BUILD_UPDATER:-OFF}
export BUILDNR=${BUILDNR:-0000}
export DESKTOP_CLIENT_ROOT=${DESKTOP_CLIENT_ROOT:-/home/user}

#Set Qt-5.15
export QT_BASE_DIR=/opt/qt5.15

export QTDIR=$QT_BASE_DIR
export PATH=$QT_BASE_DIR/bin:$PATH
export LD_LIBRARY_PATH=$QT_BASE_DIR/lib/x86_64-linux-gnu:$QT_BASE_DIR/lib:$LD_LIBRARY_PATH
export PKG_CONFIG_PATH=$QT_BASE_DIR/lib/pkgconfig:$PKG_CONFIG_PATH

# Set defaults
export SUFFIX=${DRONE_PULL_REQUEST:=master}
if [ $SUFFIX != "master" ]; then
    SUFFIX="PR-$SUFFIX"
fi
if [ "$BUILD_UPDATER" != "OFF" ]; then
    BUILD_UPDATER=ON
fi

mkdir /app

# QtKeyChain
git clone https://github.com/frankosterfeld/qtkeychain.git
cd qtkeychain
git checkout v0.10.0
mkdir build
cd build
cmake -G Ninja -D CMAKE_INSTALL_PREFIX=/app/usr ..
cmake --build . --target all
cmake --build . --target install


# Build client
mkdir build-client
cd build-client
cmake \
    -G Ninja \
    -D CMAKE_INSTALL_PREFIX=/app/usr \
    -D BUILD_TESTING=OFF \
    -D BUILD_UPDATER=$BUILD_UPDATER \
    -D MIRALL_VERSION_BUILD=$BUILDNR \
    -D MIRALL_VERSION_SUFFIX="$VERSION_SUFFIX" \
    ${DESKTOP_CLIENT_ROOT}
cmake --build . --target all
cmake --build . --target install

# Move stuff around
cd /app

mv usr/lib/x86_64-linux-gnu/* usr/lib/

mkdir usr/plugins
mv usr/lib/${APPNAME}sync_vfs_suffix.so usr/plugins
mv usr/lib/${APPNAME}sync_vfs_xattr.so usr/plugins


rm -rf usr/lib/cmake
rm -rf usr/include
rm -rf usr/mkspecs
rm -rf usr/lib/x86_64-linux-gnu/

# Don't bundle the explorer extentions as we can't do anything with them in the AppImage
rm -rf usr/share/caja-python/
rm -rf usr/share/nautilus-python/
rm -rf usr/share/nemo-python/

# Move sync exclude to right location
mv usr/etc/*/sync-exclude.lst usr/bin/
rm -rf etc

# com.nextcloud.desktopclient.nextcloud.desktop
DESKTOP_FILE=$(ls /app/usr/share/applications/*.desktop)
sed -i -e 's|Icon=nextcloud|Icon=Nextcloud|g' ${DESKTOP_FILE} # Bug in desktop file?
cp ./usr/share/icons/hicolor/512x512/apps/Nextcloud.png . # Workaround for linuxeployqt bug, FIXME


# Because distros need to get their shit together
cp -R /usr/lib/x86_64-linux-gnu/libssl.so* ./usr/lib/
cp -R /usr/lib/x86_64-linux-gnu/libcrypto.so* ./usr/lib/
cp -P /usr/local/lib/libssl.so* ./usr/lib/
cp -P /usr/local/lib/libcrypto.so* ./usr/lib/

# NSS fun
cp -P -r /usr/lib/x86_64-linux-gnu/nss ./usr/lib/

# Use linuxdeployqt to deploy
wget --ca-directory=/etc/ssl/certs -c "https://github.com/probonopd/linuxdeployqt/releases/download/continuous/linuxdeployqt-continuous-x86_64.AppImage"
chmod a+x linuxdeployqt*.AppImage
./linuxdeployqt-continuous-x86_64.AppImage --appimage-extract
rm ./linuxdeployqt-continuous-x86_64.AppImage
unset QTDIR; unset QT_PLUGIN_PATH ; unset LD_LIBRARY_PATH
export LD_LIBRARY_PATH=/app/usr/lib/
./squashfs-root/AppRun ${DESKTOP_FILE} -bundle-non-qt-libs -qmldir=${DESKTOP_CLIENT_ROOT}/src/gui

# Set origin
./squashfs-root/usr/bin/patchelf --set-rpath '$ORIGIN/' /app/usr/lib/lib${APPNAME}sync.so.0

# Build AppImage
./squashfs-root/AppRun ${DESKTOP_FILE} -appimage -updateinformation="gh-releases-zsync|nextcloud-releases|desktop|latest|Nextcloud-*-x86_64.AppImage.zsync"

#move AppImage
if [ ! -z "$DRONE_COMMIT" ]
then
    mv Nextcloud*.AppImage Nextcloud-${SUFFIX}-${DRONE_COMMIT}-x86_64.AppImage
fi
mv *.AppImage ${DESKTOP_CLIENT_ROOT}/
