#!/bin/sh

# SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
# SPDX-FileCopyrightText: 2015 ownCloud GmbH
# SPDX-License-Identifier: GPL-2.0-or-later

# Always enable the new 10.10 finder plugin if available
if [ -x "$(command -v pluginkit)" ]; then
    # add it to DB. This happens automatically too but we try to push it a bit harder for issue #3463
    pluginkit -a  "/Applications/@APPLICATION_NAME@.app/Contents/PlugIns/FinderSyncExt.appex/"
    # Since El Capitan we need to sleep #4650
    sleep 10s
    # enable it
    pluginkit -e use -i @APPLICATION_REV_DOMAIN@.FinderSyncExt
fi

exit 0
