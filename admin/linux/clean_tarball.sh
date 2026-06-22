#!/bin/sh

# SPDX-FileCopyrightText: 2017 ownCloud GmbH
# SPDX-License-Identifier: GPL-2.0-or-later

rm -vrf admin/
rm -vrf src/3rdparty/sqlite3   # FIXME: For CentOS6 we have to use our bundled sqlite
rm -vrf binary/
rm -vrf shell_integration/windows
rm -vrf shell_integration/MacOSX
