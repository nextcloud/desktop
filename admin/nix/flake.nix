/*
  SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
  SPDX-License-Identifier: GPL-2.0-or-later
*/

{
  description = "A flake for the Nextcloud desktop client";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs =
    {
      self,
      nixpkgs,
      flake-utils,
    }:
    with flake-utils.lib;
    eachSystem [ "aarch64-linux" "x86_64-linux" "aarch64-darwin" "x86_64-darwin" ] (
      system:
      let
        pkgs = import nixpkgs {
          inherit system;
        };

        inherit (pkgs.lib.lists) optionals;
        inherit (pkgs.lib.strings) optionalString;

        buildMacOSSymlinks = pkgs.runCommand "nextcloud-build-symlinks" { } ''
          mkdir -p $out/bin
          ln -s /usr/bin/xcrun /usr/bin/xcodebuild /usr/bin/iconutil $out/bin
        '';

        nativeBuildInputs =
          with pkgs;
          [
            cmake
            inkscape
            pkg-config

            qt6Packages.wrapQtAppsHook

            kdePackages.extra-cmake-modules
          ]
          ++ optionals stdenv.hostPlatform.isDarwin [
            buildMacOSSymlinks
          ];

        buildInputs =
          with pkgs;
          [
            kdsingleapplication
            libp11
            libsysprof-capture
            openssl
            pcre2
            sqlite

            qt6Packages.qt5compat
            qt6Packages.qtbase
            qt6Packages.qtdeclarative
            qt6Packages.qtkeychain
            qt6Packages.qtsvg
            qt6Packages.qttools
            qt6Packages.qtwayland
            qt6Packages.qtwebsockets

            kdePackages.karchive
          ]
          ++ optionals stdenv.hostPlatform.isLinux [
            inotify-tools
            libcloudproviders
            libsecret

            kdePackages.breeze-icons
            kdePackages.kio
            kdePackages.qqc2-desktop-style
          ]
          ++ optionals stdenv.hostPlatform.isDarwin [
            darwin.apple_sdk.frameworks.UserNotifications
          ];

        packages.default =
          with pkgs;
          stdenv.mkDerivation {
            inherit nativeBuildInputs buildInputs;
            pname = "nextcloud-client";
            version = "dev";
            src = ../../.;

            dontStrip = true;
            enableDebugging = true;
            separateDebugInfo = false;
            enableParallelBuilding = true;

            cmakeFlags = [
              "-DBUILD_UPDATER=off"
              "-DMIRALL_VERSION_SUFFIX=" # remove git suffix from version
            ]
            ++ optionals stdenv.hostPlatform.isLinux [
              "-DCMAKE_INSTALL_LIBDIR=lib" # expected to be prefix-relative by build code setting RPATH
              "-DNO_SHIBBOLETH=1" # allows to compile without qtwebkit
            ]
            ++ optionals stdenv.hostPlatform.isDarwin [
              "-DQT_ENABLE_VERBOSE_DEPLOYMENT=TRUE"
              "-DBUILD_OWNCLOUD_OSX_BUNDLE=OFF"
            ];
            postPatch = ''
              substituteInPlace src/common/utility_unix.cpp \
                --replace-fail \
                  'QLatin1String("Exec=\"") << executablePath << "\" --background\n"' \
                  'QLatin1String("Exec=\"") << "nextcloud --background\n"'
            ''
            + optionalString stdenv.hostPlatform.isLinux ''
              substituteInPlace CMakeLists.txt \
                --replace-fail '"''${SYSTEMD_USER_UNIT_DIR}"' "\"$out/lib/systemd/user\""

              for file in src/libsync/vfs/*/CMakeLists.txt; do
                substituteInPlace $file \
                  --replace-fail "PLUGINDIR" "KDE_INSTALL_PLUGINDIR"
              done
            '';
            qtWrapperArgs = [
              "--prefix LD_LIBRARY_PATH : ${lib.makeLibraryPath [ libsecret ]}"
              # make xdg-open overridable at runtime
              "--suffix PATH : ${lib.makeBinPath [ xdg-utils ]}"
              "--set QML_DISABLE_DISK_CACHE 1"
            ];
          };

        apps.default = mkApp {
          name = "nextcloud";
          drv = packages.default;
        };

      in
      {
        inherit packages apps;
        devShell = pkgs.mkShell {
          inherit buildInputs;
          nativeBuildInputs =
            with pkgs;
            nativeBuildInputs
            ++ optionals (stdenv.hostPlatform.isLinux) [
              gdb
              qtcreator
            ];
          name = "nextcloud-client-dev-shell";
        };
      }
    );
}
