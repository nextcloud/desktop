#!/bin/sh

# this script creates a plugin for nemo, just be replacing
# all occurences of Nautilus with Nemo.

sed -i.org -e 's/autilus/emo/g' syncstate_nemo.py
