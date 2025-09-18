#!/bin/sh

# SPDX-FileCopyrightText: 2016 ownCloud GmbH
# SPDX-License-Identifier: GPL-2.0-or-later

# this script creates a plugin for caja, just by replacing
# all occurrences of Nautilus with Caja (case sensitive).

cp syncstate.py syncstate_caja.py
sed -i.org -e 's/Nautilus/Caja/g' syncstate_caja.py
sed -i.org -e 's/nautilus/caja/g' syncstate_caja.py
