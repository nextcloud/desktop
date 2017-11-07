#!/bin/bash

set -e -u

scriptdir=`dirname $0`

. "${scriptdir}/config.sh"

distribution="${1}"
shift

resultdir="${PBUILDER_ROOT}/${distribution}_result"

rm -f "${PBUILDER_DEPS}/"*.deb
echo -n > "${PBUILDER_DEPS}/Packages"
rm -f "${resultdir}/"*

source "${HOME}/.pbuilderrc"

dscversion=`echo ${NEXTCLOUD_CLIENT_FULL_VERSION} | sed "s:@DISTRIBUTION@:${distribution}:g"`
pbuilder-dist "${distribution}" build --othermirror "${OTHERMIRROR}" --debbuildopts "-j${NUMCPUS}" "$@" "${BUILDAREA}/nextcloud-client_${dscversion}.dsc"
