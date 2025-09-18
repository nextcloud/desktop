#!/bin/sh

# SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
# SPDX-FileCopyrightText: 2014 ownCloud GmbH
# SPDX-License-Identifier: GPL-2.0-or-later

# kill the old version. see issue #2044
killall @APPLICATION_EXECUTABLE@
killall @APPLICATION_NAME@

exit 0
