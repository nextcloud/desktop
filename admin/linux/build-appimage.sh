#! /bin/bash

set -xe

export BUILDNR=${BUILDNR:-0000}
export APPNAME=${APPNAME:-nextcloud}
export BUILD_UPDATER=${BUILD_UPDATER:-OFF}
export BUILD_TYPE=${BUILD_TYPE:-Release}

mkdir /app
mkdir /build

export CC=/usr/bin/gcc
export CXX=/usr/bin/g++

#Set Qt-5.12
export QT_BASE_DIR=/opt/qt5.12.10
export QTDIR=$QT_BASE_DIR
export PATH=$QT_BASE_DIR/bin:$PATH
export LD_LIBRARY_PATH=$QT_BASE_DIR/lib/x86_64-linux-gnu:$QT_BASE_DIR/lib:$LD_LIBRARY_PATH
export PKG_CONFIG_PATH=$QT_BASE_DIR/lib/pkgconfig:$PKG_CONFIG_PATH

# Set defaults
if [ "$BUILD_UPDATER" != "OFF" ]; then
    BUILD_UPDATER=ON
fi

#QtKeyChain master
cd /build
git clone https://github.com/frankosterfeld/qtkeychain.git
cd qtkeychain
git checkout v0.10.0
mkdir build
cd build
cmake -D CMAKE_INSTALL_PREFIX=/usr -D CMAKE_BUILD_TYPE=${BUILD_TYPE} ../
make -j$(nproc)
make -j$(nproc) DESTDIR=/app install
make -j$(nproc) install


#Build client
cd /build
mkdir build-client
cd build-client
cmake -D CMAKE_INSTALL_PREFIX=/usr \
    -D NO_SHIBBOLETH=1 \
    -D BUILD_TESTING=OFF \
    -D BUILD_UPDATER=$BUILD_UPDATER \
    -D QTKEYCHAIN_LIBRARY=/app/usr/lib/x86_64-linux-gnu/libqt5keychain.so \
    -D QTKEYCHAIN_INCLUDE_DIR=/app/usr/include/qt5keychain/ \
    -D MIRALL_VERSION_BUILD=$BUILDNR \
    -D CMAKE_BUILD_TYPE=${BUILD_TYPE} \
    /home/user/
make -j$(nproc)
make -j$(nproc) DESTDIR=/app install

# Move stuff around
cd /app

if [ -d "./usr/lib/x86_64-linux-gnu/${APPNAME}" ]; then
    mv ./usr/lib/x86_64-linux-gnu/${APPNAME}/* ./usr/lib/x86_64-linux-gnu/
fi
mv ./usr/lib/x86_64-linux-gnu/* ./usr/lib/
if [ -d "./usr/lib/${APPNAME}" ]; then
    rm -rf ./usr/lib/${APPNAME}
fi
rm -rf ./usr/lib/cmake
rm -rf ./usr/include
rm -rf ./usr/mkspecs
rm -rf ./usr/lib/x86_64-linux-gnu/

# Don't bundle nextcloudcmd as we don't run it anyway
rm -rf ./usr/bin/${APPNAME}cmd

# Don't bundle the explorer extentions as we can't do anything with them in the AppImage
rm -rf ./usr/share/caja-python/
rm -rf ./usr/share/nautilus-python/
rm -rf ./usr/share/nemo-python/

# Move sync exclude to right location
mv ./etc/*/sync-exclude.lst ./usr/bin/
rm -rf ./etc

# com.nextcloud.desktopclient.nextcloud.desktop
DESKTOP_FILE=$(ls /app/usr/share/applications/*.desktop)
sed -i -e 's|Icon=nextcloud|Icon=Nextcloud|g' ${DESKTOP_FILE} # Bug in desktop file?
cp ./usr/share/icons/hicolor/512x512/apps/Nextcloud.png . # Workaround for linuxeployqt bug, FIXME


# Because distros need to get their shit together
cp -R /lib/x86_64-linux-gnu/libssl.so* ./usr/lib/
cp -R /lib/x86_64-linux-gnu/libcrypto.so* ./usr/lib/
cp -P /usr/local/lib/libssl.so* ./usr/lib/
cp -P /usr/local/lib/libcrypto.so* ./usr/lib/

# NSS fun
cp -P -r /usr/lib/x86_64-linux-gnu/nss ./usr/lib/

# Use linuxdeployqt to deploy
cd /build
wget --ca-directory=/etc/ssl/certs -c "https://github.com/probonopd/linuxdeployqt/releases/download/continuous/linuxdeployqt-continuous-x86_64.AppImage"
chmod a+x linuxdeployqt*.AppImage
./linuxdeployqt-continuous-x86_64.AppImage --appimage-extract
rm ./linuxdeployqt-continuous-x86_64.AppImage
unset QTDIR; unset QT_PLUGIN_PATH ; unset LD_LIBRARY_PATH
export LD_LIBRARY_PATH=/app/usr/lib/
./squashfs-root/AppRun ${DESKTOP_FILE} -bundle-non-qt-libs -qmldir=/home/user/src/gui

# Set origin
./squashfs-root/usr/bin/patchelf --set-rpath '$ORIGIN/' /app/usr/lib/lib${APPNAME}sync.so.0

# Build AppImage
./squashfs-root/AppRun ${DESKTOP_FILE} -appimage

#move AppImage
mv /build/*.AppImage /home/user/
