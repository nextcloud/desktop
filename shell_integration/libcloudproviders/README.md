<!--
  - SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
  - SPDX-License-Identifier: GPL-2.0-or-later
-->

# CloudProviders integration

Nextcloud publishes CloudProviders metadata and exports its accounts while the client is running. File managers and GTK file dialogs can discover a client started manually or at login. The integration does not install a D-Bus activation service, so discovering providers does not launch Nextcloud.

The desktop entry declares the provider. CloudProviders integration requires libcloudproviders 0.3.3 or later.

## Upgrading existing installations

Packages must drop the previously shipped `dbus-1/services/com.nextcloudgmbh.Nextcloud.service` file from their file lists so package upgrades remove it. Branded builds use their corresponding provider bus name.

When installing from source over the same prefix, the installer removes an exact match for the old Nextcloud activation service. If an old service remains in a custom location, verify that it belongs to the previous Nextcloud installation before removing it.

## Regression tests

With CMake, a C compiler, pkg-config, the libcloudproviders/GIO development files and `dbus-run-session` installed, run from the repository root:

```sh
cmake -S test/cloudproviders -B build-cloudproviders
cmake --build build-cloudproviders
ctest --test-dir build-cloudproviders --output-on-failure
```

The test stages the desktop entry on a private session bus, checks that the provider cannot be activated, and verifies that an existing collector discovers a provider started manually.
