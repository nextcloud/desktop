#! /bin/bash

set -xe

export APPNAME=${APPNAME:-Nextcloud}
export EXECUTABLE_NAME=${EXECUTABLE_NAME:-nextcloud}
export BUILD_UPDATER=${BUILD_UPDATER:-OFF}
export BUILDNR=${BUILDNR:-0000}
export DESKTOP_CLIENT_ROOT=${DESKTOP_CLIENT_ROOT:-/home/user}

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
    -DQT_MAJOR_VERSION=6 \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DBUILD_TESTING=OFF \
    -DBUILD_UPDATER=$BUILD_UPDATER \
    -DMIRALL_VERSION_BUILD=$BUILDNR \
    -DMIRALL_VERSION_SUFFIX="$VERSION_SUFFIX" \
    -DCMAKE_UNITY_BUILD=ON \
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

# Use linuxdeploy to deploy
export APPIMAGE_NAME=linuxdeploy-x86_64.AppImage
wget -O ${APPIMAGE_NAME} --ca-directory=/etc/ssl/certs -c "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
chmod a+x ${APPIMAGE_NAME}
./${APPIMAGE_NAME} --appimage-extract
rm ./${APPIMAGE_NAME}
cp -r ./squashfs-root ./linuxdeployqt-squashfs-root
export LD_LIBRARY_PATH=/app/usr/lib:/usr/local/lib/x86_64-linux-gnu:/usr/local/lib:/usr/local/lib64
./squashfs-root/AppRun --desktop-file=${DESKTOP_FILE} --icon-file=usr/share/icons/hicolor/512x512/apps/${APPNAME}.png --executable=usr/bin/${EXECUTABLE_NAME} --appdir=AppDir --output appimage

# Use linuxdeploy-plugin-qt to deploy qt dependencies
#export APPIMAGE_NAME=linuxdeploy-plugin-qt-x86_64.AppImage
#wget -O ${APPIMAGE_NAME} --ca-directory=/etc/ssl/certs -c "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage"
#chmod a+x ${APPIMAGE_NAME}
#./${APPIMAGE_NAME} --appimage-extract
#rm ./${APPIMAGE_NAME}
#cp -r ./squashfs-root ./linuxdeployqt-squashfs-root
#./squashfs-root/AppRun --appdir AppDir

# Set origin
#./squashfs-root/usr/bin/patchelf --set-rpath '$ORIGIN/' /app/usr/lib/lib*sync.so.0

# Build AppImage
#./squashfs-root/AppRun ${DESKTOP_FILE} -appimage -updateinformation="gh-releases-zsync|nextcloud-releases|desktop|latest|Nextcloud-*-x86_64.AppImage.zsync"

# Workaround issue #103
#rm -rf ./squashfs-root
#APPIMAGE=$(ls *.AppImage)
#"./${APPIMAGE}" --appimage-extract
#rm "./${APPIMAGE}"
#rm ./squashfs-root/usr/lib/libglib-2.0.so.0
#rm ./squashfs-root/usr/lib/libgobject-2.0.so.0
#PATH=./linuxdeployqt-squashfs-root/usr/bin:$PATH appimagetool -n ./squashfs-root "$APPIMAGE"

#move AppImage
if [ ! -z "$DRONE_COMMIT" ]
then
    mv *.AppImage ${EXECUTABLE_NAME}-${SUFFIX}-${DRONE_COMMIT}-x86_64.AppImage
else
    mv *.AppImage ${EXECUTABLE_NAME}-${SUFFIX}-x86_64.AppImage
fi
mv *.AppImage ${DESKTOP_CLIENT_ROOT}/
