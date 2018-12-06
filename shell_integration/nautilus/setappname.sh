#!/bin/sh

# this script replaces the line
#  appname = 'Nextcloud'
# with the correct branding name in the syncstate.py script
sed -i.org -e 's/appname\s*=\s*'"'"'Nextcloud'"'/appname = '$1'/" syncstate.py
