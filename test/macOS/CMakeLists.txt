# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

# Pins the identifiers the FinderSync XPC rendezvous depends on. Reads no bundle and starts
# no GUI, so it runs in PR CI — unlike the signed-bundle checks, which need a signing identity
# and a login session and therefore belong to the release pipeline.
nextcloud_add_test(FinderSyncBrokerIdentity)
