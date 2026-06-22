#!/bin/sh

# SPDX-FileCopyrightText: 2016 ownCloud GmbH
# SPDX-License-Identifier: GPL-2.0-or-later

# this script creates a plugin for nemo, just be replacing
# all occurrences of Nautilus with Nemo.

cp syncstate.py syncstate_nemo.py
sed -i.org -e 's/autilus/emo/g' syncstate_nemo.py
