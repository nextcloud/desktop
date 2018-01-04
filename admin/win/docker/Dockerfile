FROM opensuse:42.1

MAINTAINER Daniel Molkentin <danimo@owncloud.com>

ENV TERM ansi
ENV HOME /root

ENV REFRESHED_AT 20160421

RUN zypper --non-interactive --gpg-auto-import-keys refresh
RUN zypper --non-interactive --gpg-auto-import-keys ar http://download.opensuse.org/repositories/windows:/mingw/openSUSE_Leap_42.1/windows:mingw.repo
RUN zypper --non-interactive --gpg-auto-import-keys ar http://download.opensuse.org/repositories/isv:ownCloud:toolchains:mingw:win32:2.3/openSUSE_Leap_42.1/isv:ownCloud:toolchains:mingw:win32:2.3.repo
RUN zypper --non-interactive --gpg-auto-import-keys install cmake make mingw32-cross-binutils mingw32-cross-cpp mingw32-cross-gcc \
                      mingw32-cross-gcc-c++ mingw32-cross-pkg-config mingw32-filesystem \
                      mingw32-headers mingw32-runtime site-config mingw32-libwebp mingw32-libssp0 \
                      mingw32-cross-libqt5-qmake mingw32-cross-libqt5-qttools mingw32-libqt5* \
                      mingw32-qt5keychain* mingw32-angleproject* \
                      mingw32-cross-nsis mingw32-libopenssl* \
                      mingw32-sqlite* kdewin-png2ico \
                      osslsigncode wget

# RPM depends on curl for installs from HTTP
RUN zypper --non-interactive --gpg-auto-import-keys install curl

# sudo needed for building as user
RUN zypper --non-interactive --gpg-auto-import-keys install sudo

# Use packaged UAC dependencies
RUN zypper --non-interactive --gpg-auto-import-keys install mingw32-cross-nsis-plugin-uac mingw32-cross-nsis-plugin-nsprocess

# Required for checksumming
RUN zypper --non-interactive --gpg-auto-import-keys install mingw32-zlib-devel

# Required for windres not to crash
RUN zypper --non-interactive --gpg-auto-import-keys install glibc-locale

CMD /bin/bash
