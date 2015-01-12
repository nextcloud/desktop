#!/bin/sh

# this script replaces the line
#  appname = 'ownCloud'
# with the correct branding name in the syncstate.py script
/usr/bin/sed -i.org -e 's/appname\s*=\s*'"'"'ownCloud'"'/appname = '$1'/" syncstate.py
