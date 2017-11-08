#!/bin/bash
# Copyright (C) 2017 Marco Trevisan

set -xe

TRAVIS_BUILD_STEP="$1"
THIS_PATH=$(dirname $0)

if [ -z "$TRAVIS_BUILD_STEP" ]; then
    echo "No travis build step defined"
    exit 0
fi

if [ "$BUILD_TYPE" == "debian" ]; then
    tools/linux/debian/travis-build.sh "$@"
else
    echo 'No $BUILD_TYPE defined'
    exit 1
fi
