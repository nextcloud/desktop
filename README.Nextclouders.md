# Changes and new features

## Hello Nextclouders! :-)

I've made some adjustments to the original build steps and hope they ease your life when creating
daily builds.

Feel free to adopt and share.

## New features

Important: See README in this repo for detailed build instructions and explanations.

- Support for 32-bit and 64-bit Windows
- Installer now builds one package containing Win32 + Win64 (program files path detection corrected)
- Code Signing now also happens for nextcloud.exe, the Uninstaller and other files (zlib, OpenSSL, etc.)
- Dedicated build script for collecting and signing all required client files
  - Allows to create local and portable working clients without the need for installation
  - Eases the transition to another installation system (e.g. MSI) in the future
- There is new directory (created by init.bat) for extra deployment of required files (e.g. SSL DLLs)
  with a structure for all build targets (Release, Debug) and architectures (Win64, Win32).
  You may add arbitrary files there. They get collected by the build scripts which store them in
  the installer package for the correct target system version.
- Project path can now also be specified outside the client-building repo.
- See README for a more detailed list of new configuration options and environment variables

## Changes

- Had to rename the shell extension DLLs to make them work properly on different verions of Windows since
  in some cased the underscore caused trouble with the DLL registration
- Added an extended version suffix for the client build to distinguish between Win64 and Win32 versions
- Lots of minor / medium improvements, e.g.:
  - no need to manually specify the path to signtool.exe anymore (see: defaults.inc.bat)
  - supply windeployqt parameter to deploy the correct files and libs for --release OR --debug
  - changed the URLs in nextcloud.msi from http:// to https://

## TODO:

- Have a look at nextcloud.nsi and determine whether this section is still required:

      ; TODO: Only needed to when updating from 2.1.{0,1}. Remove in due time.
      Delete /REBOOTOK $INSTDIR\bearer\qgenericbearer.dll
      Delete /REBOOTOK $INSTDIR\bearer\qnativewifibearer.dll
      RMDir /REBOOTOK $INSTDIR\bearer

  I was really confused at which point the "bearer" directory got lost in the build until I discovered
  this section in the NSIS script.

- Update the outdated docs everywhere to point to the correct build instructions (to the client-building repo)
  - e.g. here: https://github.com/nextcloud/desktop/wiki/System-requirements-for-compiling-the-desktop-client
  - and here: https://doc.owncloud.org/desktop/2.5/building.html

# Old content from the original client-building repo:

Partially obsolete, kept for the archives / Nextcloud's internal build proccess.

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

