#!/bin/bash

set -e -u

scriptdir=`dirname $0`
scriptdir=`cd "${scriptdir}" && pwd`

. "${scriptdir}/config.sh"

distribution="${1}"
shift

pushd /
"${scriptdir}/build.sh" "${distribution}" -S "$@"

"${scriptdir}/pbuilder.sh" "${distribution}" "$@"
popd
