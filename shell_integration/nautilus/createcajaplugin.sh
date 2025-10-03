#!/bin/sh

# this script creates a plugin for caja, just by replacing
# all occurences of Nautilus with Caja (case sensitive).

cp syncstate.py syncstate_caja.py
sed -i.org -e 's/Nautilus/Caja/g' syncstate_caja.py
sed -i.org -e 's/nautilus/caja/g' syncstate_caja.py
