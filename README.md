# How to build the Nextcloud desktop client on Windows

This allows you to easily build the desktop client for 64-bit and 32-bit Windows.

## Update: 2019-08-18

Qt 5.12.4 for Windows doesn't require the old 1.0.x DLL's anymore (libeay32.dll + ssleay32.dll)
and is linked against OpenSSL 1.1.1c

This finally removes the odd mixture of diverging DLL versions.
Now the Desktop Client finally uses OpenSSL 1.1.1 components only, reports the correct
library version and also supports TLS 1.3 :-)

See here for details: https://blog.qt.io/blog/2019/06/17/qt-5-12-4-released-support-openssl-1-1-1/

- Code Signing of the client binaries (exe + dll's), the installer and the uninstaller
- Uploading the generated installer package via SSH connection (scp)
- Script file to be invoked by the Windows Task Scheduler for automatic builds (e.g.: daily)

## Included and fully automated
- Build client for 64-bit and 32-bit Windows (+ installer package containing both)
- Code Signing of the client binaries (exe + dll's), the installer and the uninstaller
- Uploading the generated installer package via SSH connection (scp)
- Script file to be invoked by the Windows Task Scheduler for automatic builds (e.g.: daily)

## Motivation
- Everybody should be able to build the Windows client, the build scripts and guide here
  should help to make this a pain-free experience.
- It's designed to be a drop-in replacement for the existing build scripts, used to create
  the official Nextcloud releases.

## System requirements
- Windows 10 / 8 / 7, 64-bit (bare metal or virtual machine, Win 10 + Win 7 successfully tested)
- All the tools from the Install list below
- Internet connection for each build
- At least 10 GB disk space for the build files and more gigs for the required tools

Optional:
- Code Signing certificate, if you intend to sign the binaries (you may use a self-generated one for testing)
- SSH server and credentials for the upload to work

## General
- Grab a cup of coffee
- Download of the Windows 10 x64 VM: https://developer.microsoft.com/en-us/microsoft-edge/tools/vms/
- See README.Nextclouders in this repo for a list of noteworthy changes I made to the original build
  scripts found at nextcloud/client-building

### Assumed working folder
- C:\Nextcloud

  - Warning: Don't use white spaces in the build (project) paths, since this breaks support for some tools like png2ico !
  - If you prefer to, you may specify a project directory outside of the cloned client-building repo.

### Install list
- [ ] Visual Studio 2017 (select in the wizard: "Desktop Development with C++"):
      https://visualstudio.microsoft.com/thank-you-downloading-visual-studio/?sku=Community&rel=15

- [ ] Git bash (it comes with Git):
      https://git-scm.com/download/win

- [ ] Qt 5.12.4 (select in the wizard: MSVC 2017 64-bit AND 32-bit and all the "Qt ..." options, not required: Debug Info Files):
      https://www.qt.io/download

      Install to: C:\Qt

- [ ] CMake 3.14.x (choose the ZIP version, extract and rename to: C:\Nextcloud\tools\cmake):
      https://cmake.org/download/

- [ ] Png2Icon - you need to use this version: https://github.com/hiiamok/png2ImageMagickICO

      Put the generated png2ico.exe to: C:\Nextcloud\tools\

- [ ] OpenSSL: Windows Build:
      https://slproweb.com/products/Win32OpenSSL.html     (e.g.: Win64 AND Win32 OpenSSL v1.1.1c)

      Install to:
      - C:\OpenSSL\Win64
      - C:\OpenSSL\Win32

    Note: Qt 5.12.4 also includes the option to install OpenSSL 1.1.1c libraries from the Maintenance tool wizard.
          You may also use these libraries instead of the ones above but then you have to modify the paths in defaults.inc.bat
          and be sure to check for updates on a regular basis!

## NSIS (Nullsoft Scriptable Install System)
- Install NSIS: https://nsis.sourceforge.io/Download/

## NSIS plugins to install
- [ ] https://nsis.sourceforge.io/UAC_plug-in
- [ ] https://nsis.sourceforge.io/NsProcess_plugin

## To upload builds
- https://success.tanaza.com/s/article/How-to-use-SCP-command-on-Windows
- Hint: Don't forget to manually connect via ssh for the first time in order to trust the host key!

## Build the client: Initial setup
This has to be done ONLY ONCE to create the build folder structure and fetch the required
repos (qtkeychain, zlib and Nextcloud's desktop):

- Create folders:
  - C:\Nextcloud
  - C:\Nextcloud\tools

- Open Git Bash:
  ```
  cd /c/Nextcloud
  git clone https://github.com/misch7/nextcloud-client-building.git client-building
  ```

- Take a look at this file and adapt the environment variables to your needs. You may also define them in the Windows
  Environment Variables settings, if you don't want modify the file directly:

  - C:\Nextcloud\client-building\defaults.inc.bat

- Again in Git Bash:
  ```
  cd client-building
  ./init.bat
  ```

- Extract the OpenSSL Pre-compiled Win32/64 libraries to the newly created directories:

  - C:\Nextcloud\client-building\deploy-extra\[Release + Debug]\[Win64 + Win32]\

- Recommended: Do a test run to check for all the required environment variables:
  ```
  TEST_RUN=1 ./build.bat
  ```
  Note: This only tests for the existence of all variables, no other file or folder checks
        will be perfomed.

If the real build fails the scripts will stop and return an error code which is pretty helpful
for automated builds. It appears for example in the Task Scheduler.
The console output or the log file (only with task-build-log.bat) ease tracing the error's source.

However they complain if there are missing environment variables in order to save time,
so you don't have to wait for half completion of the build. ;-)


## Build the client: Perform the actual build
When the initial setup is done, only the following needs to be done if you want to
build the client at any time later:

- In Git Bash again:
  ```
  ./build.bat
  ```
  - OR instead, if you want the output in a log file:
  ```
  ./task-build-log.bat
  ```

- By default the commands above will build Release for 64-bit and 32-bit.

### Specifying build types (Release, Debug)
- Examples:
  ```
  ./build.bat Release
  ```
  ```
  ./task-build-log.bat Debug
  ```

- To build Release, collect all files but build no installer, don't sign and don't upload:
  ```
  BUILD_INSTALLER=0 USE_CODE_SIGNING=0 UPLOAD_BUILD=0 ./build.bat Release
  ```

## Explanation of the build scripts
- build.bat invokes all the other build scripts, stages are:

  1) build-qtkeychain.bat
     - builds the Qt Keychain library (https://github.com/frankosterfeld/qtkeychain)

  2) build-zlib.bat
     - builds the latest stable zlib (https://github.com/madler/zlib)

  3) build-desktop.bat
     - builds the Nextcloud Desktop Sync Client (https://github.com/nextcloud/desktop)

  4) build-installer-collect.bat
     - collects all the required files and libs for the client and signs some of them

  5) build-installer-exe.bat
     - builds and signs the combined installer package for Win64 + Win32

You may run any of them directly if build.bat failed and you resolved the error.
But they need to be run in the correct order above and all the previous scripts
should have succeeded.
Parameter syntax is the same as for build.bat.

These scripts contain a loop which invoke their single-build-*.bat helpers and they
also set the required environment variables you normally would have to specify manually.
Exception: build-installer-exe.bat doesn't need a loop since it builds a combined
installer package, so there is no single-build-installer-exe.bat.

Of course you could do this, e.g. calling syntax to only build for 32-bit would be:
```
./single-build-desktop.bat Release Win32
```

Upon every new invocation the build scripts fetch the newest version of their repo.

Take a look at the scripts if you want to know how they work, which resources they
collect and the CMake options and generators they use.

- init.bat
  - clones the GitHub repos and creates required directories

- defaults.inc.bat
   - sets all required variables if not already present in the environment

- common.inc.bat
  - determines the build type (Release or Debug) and sets the CMake generator:
    - 64-bit: "Visual Studio 15 2017 Win64"
    - 32-bit: "Visual Studio 15 2017"

- datetime.inc.bat + datetime.inc.callee
  - provide a locale-independent way to get a unified build date instead of using %DATE%

- build-installer-collect.bat
  - collects all required libraries and resources the client needs
    at runtime (Qt, Qt Keychain, zlib, OpenSSL, VC Redist, etc.)

- task-build-log.bat
  - is intended to by used in conjunction with the Windows Task Scheduler but may also be run
    manually instead of build.bat, which shows all the output in the console window only

- task-build-job.sh
  - is the actual script to be specified in the Windows Task Scheduler to be run by Git Bash

- sign.bat
  - used for signing the binaries, the installer and the uninstaller

- upload.bat
  - used for uploading the installer package to a server (using scp)

## Task Scheduler
Use the following values in a new task:

- Action: Start a program
  - Program/script:
    ```"C:\Program Files\Git\bin\sh.exe"```
  - Arguments:
    ```C:\Nextcloud\client-building\task-build-job.sh Release```
  - Start in:
    ```C:\Nextcloud\client-building```

OR:

- Action: Start a program
  - Program/script:
    ```cmd```
  - Arguments:
    ```/c ""C:\Program Files\Git\bin\sh.exe" C:\Nextcloud\client-building\task-build-job.sh Release"```
  - Start in:
    ```C:\Nextcloud\client-building```

## Which binaries get signed?
- The following binaries get signed by build-installer-collect.bat as we're responsible
  for them or they're critical for security and privacy:
  - all the binaries we produce:
    - nextcloud/ocsync.dll
    - shellext/OCContextMenu.dll
    - shellext/OCOverlays.dll
    - shellext/OCUtil.dll
    - nextcloud.exe
    - nextcloudcmd.exe
    - nextcloudsync.dll
    - OCContextMenu.dll
    - OCOverlays.dll
    - ocsync.dll
    - OCUtil.dll
    - qt5keychain.dll
    - zlib.dll
  - the pre-compiled OpenSSL binaries (since they are not signed but integrity is crucial)
    - libcrypto*.dll
    - libssl*.dll
  - we don't sign Qt, as of version 5.12.4 binaries are now signed by the Qt team
  - VC Runtime DLL's are already signed by Microsoft

## License

    Copyright (c) 2019 Michael Schuster
    Parts based on work at: https://github.com/nextcloud/client-building/tree/6b2d7d34f7d79ccb7fcbc6b285da1f6f8f2bbfc8

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
    or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
    for more details.
