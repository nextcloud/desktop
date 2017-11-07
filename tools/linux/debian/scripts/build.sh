#!/bin/bash

set -e -u

scriptdir=`dirname $0`
scriptdir=`cd "${scriptdir}" && pwd`

. "${scriptdir}/config.sh"

package="nextcloud-client"
tag="${1}"
version="${2}"
distribution="${3}"
shift 3

pushd /

"${scriptdir}/create_debdir.sh" "${tag}" "${version}" "${distribution}"

(cd "${BUILDAREA}/${package}_${version}"; EDITOR=true dpkg-source --commit . local-changes; debuild "$@")

popd
