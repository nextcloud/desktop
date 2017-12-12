#!/bin/bash

set -e -u

scriptdir=`dirname $0`

DEFAULT_DIST=yakkety

package="nextcloud-client"
version="$1"
comment="$2"
distver="${3:-}"

if test -z "${distver}"; then
    distver="1"
fi

packagedir="${scriptdir}/../${package}"

now=`date -R`

tmpfile="/tmp/addchange.$$"

for subdir in "${packagedir}/debian"*; do
    if test -f "${subdir}/changelog"; then
        echo "${subdir}"
        bname=`basename "${subdir}"`
        dist=`echo "${bname}" | sed 's:debian\.\?::'`
        if test -z "${dist}"; then
            dist="${DEFAULT_DIST}"
        fi
        if test "${dist}" = "stable"; then
            versuffix=""
        else
            versuffix="~${dist}${distver}"
        fi
        cat > "${tmpfile}" <<EOF
${package} (${version}${versuffix}) ${dist}; urgency=medium

  * ${comment}

 -- István Váradi <ivaradi@varadiistvan.hu>  ${now}

EOF
        cat "${subdir}/changelog" >> "${tmpfile}"
        mv "${tmpfile}" "${subdir}/changelog"
    fi
done

rm -f "${tmpfile}"
