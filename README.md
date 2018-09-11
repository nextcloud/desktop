# How to build the desktop client on Windows

## General
- Grab a cup of coffee
- Download the Windows VM: https://developer.microsoft.com/en-us/microsoft-edge/tools/vms/

### Install list
- [ ] OpenSSL: https://indy.fulgan.com/SSL/
- [ ] zlib: https://github.com/maxirmx/Dist_zlib
- [ ] qtkeychain: https://github.com/frankosterfeld/qtkeychain
- [ ] Visual Studio
- [ ] Git bash (it comes with Git)
- [ ] Png2Icon - you need to use this version: https://github.com/hiiamok/png2ImageMagickICO


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

### Missing dll's and files even after running windeployqt
- [ ] Qt5CoreD.dll (C:\Qt\Qt5.11.1\msvc2017_64\bin)
- [ ] MSVCP140D.dll (C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Redist\MSVC\14.14.26405\debug_nonredist\x64\Microsoft.VC141.DebugCRT)
- [ ] VCRUNTIME140D.dll (C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Redist\MSVC\14.14.26405\debug_nonredist\x64\Microsoft.VC141.DebugCRT)
- [ ] ocsync.dll (nc install path/bin/nextcloud)
- [ ] libcrypto-1_1-x86.dll (C:\OpenSSL-Win64\bin)
- [ ] ucrtbased.dll (C:\Windows/System32)
- [ ] C:\qt5keychain\bin\qt5keychain.dll needs to be copied to C:\Qt\5.11.1\msvc2017_64\bin
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

## SSH Agent
We are floowing this instruction:
- https://help.github.com/articles/working-with-ssh-key-passphrases/#platform-windows (see .bashrc file in the repo)
- There is also a ~/.ssh/config file with:
```
Host download.nextcloud.com
  ForwardAgent yes
```

### Task Scheduler
- We are using it to schedule the build - the task is exported to the xml DailyBuilds_v01.xml.

## ENV VARS TO SET
- [ ] OPENSSL_PATH (openssl path installation)
- [ ] P12_KEY_PASSWORD (certificate key password used to sign the installer)
- [ ] PROJECT_PATH (nextcloud source code path)
- [ ] QT_PATH (qt installation path)
- [ ] SFTP_SERVER (server url)
- [ ] SFTP_USER (server user)
- [ ] VCINSTALLDIR (Visual Code installation path)
- [ ] Png2ico_EXECUTABLE (png2ico installation path)
- [ ] QTKEYCHAIN_LIBRARY (path-to-qt5keychain-folder/lib/qt5keychain.lib)
- [ ] QTKEYCHAIN_INCLUDE_DIR (path-to-qt5keychain-folder/include/qt5keychain)
- [ ] OPENSSL_ROOT_DIR (path-to-openssl-folder) 
- [ ] OPENSSL_INCLUDE_DIR (path-to-openssl-folder/include)
- [ ] OPENSSL_LIBRARIES (path-to-openssl-folder/lib)

## To create the installer

Run: 
```
./build.bat 
```
```build.bat``` will call ```nextcloud.nsi``` that in the end will call ```upload.bat``` after signing the installer.

