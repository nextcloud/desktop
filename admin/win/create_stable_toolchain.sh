#!/bin/bash

#
# This script creates a new windows toolchain repository in OBS.
# It only works for versions that do not yet exist.
#
# Make sure to adopt the variable stableversion.

# Set the new stable version accordingly:
stableversion=2.1
targetproject="isv:ownCloud:toolchains:mingw:win32:${stableversion}"

# Create the new repo

# get the xml build description of the stable repo
xml=`osc meta prj isv:ownCloud:toolchains:mingw:win32:stable`
stable_xml="${xml/stable/$stableversion}"

echo $stable_xml

echo $stable_xml | osc meta prj -F - ${targetproject}


# now copy all packages
packs=`osc ls isv:ownCloud:toolchains:mingw:win32:stable`

for pack in $packs
do
    osc copypac isv:ownCloud:toolchains:mingw:win32:stable $pack $targetproject
done
