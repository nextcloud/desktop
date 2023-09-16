FROM opensuse/leap:15.5

ENV TERM ansi
ENV HOME /root

ENV REFRESHED_AT 20170113

RUN zypper --non-interactive --gpg-auto-import-keys refresh
RUN zypper --non-interactive --gpg-auto-import-keys ar https://download.opensuse.org/repositories/windows:/mingw:/win32/openSUSE_Leap_15.5/windows:mingw:win32.repo
RUN zypper --non-interactive --gpg-auto-import-keys install cmake make mingw32-cross-binutils mingw32-cross-cpp mingw32-cross-gcc \
                      mingw32-cross-gcc-c++ mingw32-filesystem \
                      mingw32-headers mingw32-runtime site-config mingw32-libssp0 \
                      mingw32-angleproject* \
                      mingw32-cross-nsis mingw32-libopenssl-devel mingw32-libopenssl \
                      mingw32-sqlite* \
                      wget

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
