/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

{
  description = "A flake for the Nextcloud desktop client";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    with flake-utils.lib;
    eachSystem [ "aarch64-linux" "x86_64-linux" "aarch64-darwin" "x86_64-darwin" ] (system:
        let
          pkgs = import nixpkgs {
            inherit system;
          };

          inherit (pkgs.lib.lists) optional optionals;
          inherit (pkgs.lib.strings) hasPrefix optionalString;
          isARM = hasPrefix "aarch64" system;

          buildMacOSSymlinks = pkgs.runCommand "nextcloud-build-symlinks" {} ''
            mkdir -p $out/bin
            ln -s /usr/bin/xcrun /usr/bin/xcodebuild /usr/bin/iconutil $out/bin
          '';

          nativeBuildInputs = with pkgs; [
            cmake
            extra-cmake-modules
            pkg-config
            inkscape
            qt5.wrapQtAppsHook
          ] ++ optionals stdenv.isDarwin [
            buildMacOSSymlinks
          ];

          buildInputs = with pkgs; [
            sqlite
            openssl
            pcre

            qt5.qtbase
            qt5.qtquickcontrols2
            qt5.qtsvg
            qt5.qtgraphicaleffects
            qt5.qtdeclarative
            qt5.qttools
            qt5.qtwebsockets

            libsForQt5.karchive
            libsForQt5.qtkeychain
          ] ++ optionals (!isARM) [
            # Qt WebEngine not available on ARM
            qt5.qtwebengine
          ] ++ optionals stdenv.isLinux [
            inotify-tools
            libcloudproviders
            libsecret

            libsForQt5.breeze-icons
            libsForQt5.qqc2-desktop-style
            libsForQt5.kio
          ] ++ optionals stdenv.isDarwin [
            libsForQt5.qt5.qtmacextras

            darwin.apple_sdk.frameworks.UserNotifications
          ];

          packages.default = with pkgs; stdenv.mkDerivation rec {
            inherit nativeBuildInputs buildInputs;
            pname = "nextcloud-client";
            version = "dev";
            src = ../../.;

            dontStrip = true;
            enableDebugging = true;
            separateDebugInfo = false;
            enableParallelBuilding = true;

            preConfigure = optionals stdenv.isLinux [
            ''
              substituteInPlace shell_integration/libcloudproviders/CMakeLists.txt \
                --replace "PKGCONFIG_GETVAR(dbus-1 session_bus_services_dir _install_dir)" "set(_install_dir "\$\{CMAKE_INSTALL_DATADIR\}/dbus-1/service")"
            ''
            ] ++ optionals stdenv.isDarwin [
            ''
              substituteInPlace shell_integration/MacOSX/CMakeLists.txt \
                --replace "-target FinderSyncExt -configuration Release" "-scheme FinderSyncExt -configuration Release -derivedDataPath $ENV{NIX_BUILD_TOP}/derivedData"
            ''
            ];

            cmakeFlags = optionals stdenv.isLinux [
              "-DCMAKE_INSTALL_LIBDIR=lib" # expected to be prefix-relative by build code setting RPATH
              "-DNO_SHIBBOLETH=1" # allows to compile without qtwebkit
            ] ++ optionals stdenv.isDarwin [
              "-DQT_ENABLE_VERBOSE_DEPLOYMENT=TRUE"
              "-DBUILD_OWNCLOUD_OSX_BUNDLE=OFF"
            ];
            postPatch = optionalString stdenv.isLinux ''
              for file in src/libsync/vfs/*/CMakeLists.txt; do
                substituteInPlace $file \
                  --replace "PLUGINDIR" "KDE_INSTALL_PLUGINDIR"
              done
            '';
            postFixup = optionalString stdenv.isLinux ''
              wrapProgram "$out/bin/nextcloud" \
                --set LD_LIBRARY_PATH ${lib.makeLibraryPath [ libsecret ]} \
                --set PATH ${lib.makeBinPath [ xdg-utils ]} \
                --set QML_DISABLE_DISK_CACHE "1"
            '';
          };

          apps.default = mkApp {
            name = "nextcloud";
            drv = packages.default;
          };

        in {
          inherit packages apps;
          devShell = pkgs.mkShell {
            inherit buildInputs;
            nativeBuildInputs = with pkgs; nativeBuildInputs ++ optionals (stdenv.isLinux) [
              gdb
              qtcreator
            ];
            name = "nextcloud-client-dev-shell";
          };
        }
    );
}
