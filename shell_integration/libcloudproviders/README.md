<!--
  - SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
  - SPDX-License-Identifier: GPL-2.0-or-later
-->

# CloudProviders integration

Nextcloud publishes CloudProviders metadata and exports its accounts while the client is running. File managers and GTK file dialogs can discover a client started manually or at login. The integration does not install a D-Bus activation service, so discovering providers does not launch Nextcloud.

The desktop entry declares the provider for libcloudproviders 0.3.3 and later. Older versions use the installed `cloud-providers/*.ini` file.

## Upgrading existing installations

Packages must drop the previously shipped `dbus-1/services/com.nextcloudgmbh.Nextcloud.service` file from their file lists so package upgrades remove it. Branded builds use their corresponding provider bus name.

Installing from source into an existing prefix does not remove obsolete files. Remove the old activation service from that prefix after verifying it belongs to the previous Nextcloud installation. Leaving it installed preserves the unwanted activation behavior.

## Regression tests

With CMake, C and C++ compilers, pkg-config, the libcloudproviders/GIO development files, Qt 6 DBus development files and `dbus-run-session` installed, run from the repository root:

```sh
cmake -S test/cloudproviders -B build-cloudproviders
cmake --build build-cloudproviders
ctest --test-dir build-cloudproviders --output-on-failure
```

The tests stage the integration's install rules on a private session bus, check that the provider cannot be activated, and verify that an existing collector discovers a provider started manually. They cover both desktop and legacy `.ini` metadata.
