/*
 * Copyright (C) by Claudio Cambra <claudio.cambra@nextcloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

{
  description = "A flake for the Nextcloud desktop client";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    with flake-utils.lib;
    eachSystem [ "aarch64-linux" "x86_64-linux" ] (system:
        let
          pkgs = import nixpkgs {
            inherit system;
          };

          inherit (pkgs.lib.lists) optional optionals;
          inherit (pkgs.lib.strings) hasPrefix hasSuffix;
          isMacOS = hasSuffix "darwin" system;
          isARM = hasPrefix "aarch64" system;

          nativeBuildInputs = with pkgs; [
            cmake
            extra-cmake-modules
            pkg-config
            qt5.wrapQtAppsHook
          ];

          buildInputs = with pkgs; [
            sqlite
            openssl
            pcre
            inkscape

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
          ] ++ optionals (stdenv.isLinux) [
            inotify-tools
            libcloudproviders
            libsecret

            libsForQt5.breeze-icons
            libsForQt5.qqc2-desktop-style
            libsForQt5.kio
          ];

          packages.default = with pkgs; stdenv.mkDerivation rec {
            inherit nativeBuildInputs buildInputs;
            pname = "nextcloud-client";
            version = "dev";
            src = ../../.;
            dontStrip = true;
            enableDebugging = true;
            separateDebugInfo = false;
            cmakeFlags = if(stdenv.isLinux) then [
                "-DCMAKE_INSTALL_LIBDIR=lib" # expected to be prefix-relative by build code setting RPATH
                "-DNO_SHIBBOLETH=1" # allows to compile without qtwebkit
            ] else [];
            postPatch = if(stdenv.isLinux) then ''
              for file in src/libsync/vfs/*/CMakeLists.txt; do
                substituteInPlace $file \
                  --replace "PLUGINDIR" "KDE_INSTALL_PLUGINDIR"
              done
            '' else "";
            postFixup = if(stdenv.isLinux) then ''
              wrapProgram "$out/bin/nextcloud" \
                --set LD_LIBRARY_PATH ${lib.makeLibraryPath [ libsecret ]} \
                --set PATH ${lib.makeBinPath [ xdg-utils ]} \
                --set QML_DISABLE_DISK_CACHE "1"
            '' else "";
          };

          apps.default = mkApp {
            name = "nextcloud";
            drv = packages.default;
          };

        in {
          inherit packages apps;
          devShell = pkgs.mkShell {
            inherit buildInputs;
            nativeBuildInputs = with pkgs; nativeBuildInputs ++[
              gdb
              qtcreator
            ];
            name = "nextcloud-client-dev-shell";
          };
        }
    );
}
