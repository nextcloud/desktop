# How to build the desktop client on Windows

## General
- Grab a cup of coffee
- Download the Windows VM: https://developer.microsoft.com/en-us/microsoft-edge/tools/vms/

### Install list
- [ ] OpenSSL: https://indy.fulgan.com/SSL/
- [ ] zlib: https://github.com/maxirmx/Dist_zlib
- [ ] qtkeychain: https://github.com/frankosterfeld/qtkeychain
- [ ] Visual Studio
- [ ] Git bash (it comes with the Github tool or maybe Visual Studio(?))


## CMake options

For the desktop client (Release/Debug):
```
cmake -G "Visual Studio 15 2017 Win64" .. -DCMAKE_INSTALL_PREFIX=..\..\install -DCMAKE_BUILD_TYPE=Release
```

For qtkeychain:
```
cmake -G "Visual Studio 15 2017 Win64" .. -DCMAKE_INSTALL_PREFIX=c:\qt5keychain
```
And then (Release/Debug):
```
cmake --build . --config Release --target install
```

## DLL's
- To put all dependencies together: http://doc.qt.io/qt-5/windows-deployment.html

### Missing dll's even after running windeployqt
- [ ] Qt5CoreD.dll (C:\Qt\Qt5.9.5\5.9.5\msvc2017_64\bin)
- [ ] MSVCP140D.dll (C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Redist\MSVC\14.14.26405\debug_nonredist\x64\Microsoft.VC141.DebugCRT)
- [ ] VCRUNTIME140D.dll (C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Redist\MSVC\14.14.26405\debug_nonredist\x64\Microsoft.VC141.DebugCRT)
- [ ] ocsync.dll (nc install path/bin/nextcloud)
- [ ] libcrypto-1_1-x86.dll (C:\OpenSSL-Win64\bin)
- [ ] ucrtbased.dll (C:\Windows/System32)
- [ ] MSVCR120.dll (C:\OpenSSL-Win64\bin)
- [ ] sync-exclude.lst (C:\Users\IEUser\Desktop\nc\install\config\Nextcloud)
- [ ] had to fetch the extra dll's from https://indy.fulgan.com/SSL/openssl-1.0.1h-x64_86-win64.zip (libeay32.dll and ssleay32.dll)

## NSIS
- Install NSIS: http://nsis.sourceforge.net/Download/

## NSIS plugins to install
- [ ] http://nsis.sourceforge.net/UAC_plug-in
- [ ] http://nsis.sourceforge.net/NsProcess_plugin

## To upload builds
- https://success.tanaza.com/s/article/How-to-use-SCP-command-on-Windows

## Git submodules
- shell_integration

## ENV VARS TO SET
- [ ] OPENSSL_PATH (openssl path installation)
- [ ] P12_KEY_PASSWORD (certificate key password used to sign the installer)
- [ ] PROJECT_PATH (nextcloud source code path)
- [ ] QT_PATH (qt installation path)
- [ ] SFTP_SERVER (server url)
- [ ] SFTP_USER (server user)
- [ ] SSH_SESSION (session settings + private key saved with Putty)
- [ ] VCINSTALLDIR (Visual Code installation path)

