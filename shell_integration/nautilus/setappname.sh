#!/bin/sh

# SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
# SPDX-FileCopyrightText: 2015 ownCloud GmbH
# SPDX-License-Identifier: GPL-2.0-or-later

# this script replaces the line
#  appname = 'Nextcloud'
# with the correct branding name in the syncstate.py script
# It also replaces the occurrences in the class name so several
# branding can be loaded (see #6524)
sed -i.org -e "s/Nextcloud/$1/g" syncstate.py
