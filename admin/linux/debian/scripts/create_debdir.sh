#!/bin/bash

set -e -u

scriptdir=`dirname $0`
scriptdir=`cd "${scriptdir}" && pwd`

. "${scriptdir}/config.sh"

package="nextcloud-client"
tag="${1}"
version="${2}"
distribution="${3}"

gitdir="${GITROOTS}/client"
packagedir="${BUILDAREA}/${package}_${version}"
origtarname="${package}_${version}.orig.tar.bz2"
origtar="${BUILDAREA}/${origtarname}"

archive_submodules()
{
    local subdir="${1}"
    local treeish="${2}"

    local dir="${gitdir}"
    local destdir="${packagedir}"
    if test "${subdir}"; then
        echo "  copying submodule ${subdir}"
        dir="${dir}/${subdir}"
        destdir="${destdir}/${subdir}"
    fi

    mkdir -p "${destdir}"

    (cd "${dir}"; git archive "${treeish}" | tar xf - -C "${destdir}")

    (cd "${dir}"; git ls-tree "${treeish}" -r) | while read mode type object file; do
        if test "${type}" = "commit"; then
            sdir="${file}"
            if test "${subdir}"; then
                sdir="${subdir}/${sdir}"
            fi
            archive_submodules "${sdir}" "${object}"
        fi
    done
}

rm -rf "${packagedir}"
mkdir -p "${packagedir}"

echo "Updating submodules"
commit=`cd "${gitdir}"; git rev-parse HEAD`
(cd "${gitdir}"; git checkout "${tag}"; git submodule update --recursive --init)

echo "Copying sources"
archive_submodules "" "${tag}"

if test -f "${GITROOTS}/${origtarname}"; then
    echo "Copying orig archive from ${GITROOTS}"
    cp -a "${GITROOTS}/${origtarname}" "${BUILDAREA}"
else
    echo "Creating orig archive"
    tar cjf "${origtar}" -C "${BUILDAREA}" "${package}_${version}"
fi

echo "Restoring Git state"
(cd "${gitdir}"; git checkout "${commit}")
cd "${scriptdir}"

echo "Copying Debian files"
mkdir -p "${packagedir}/debian"
tar cf - -C "${scriptdir}/../debian" . | tar xf - -C "${packagedir}/debian"

if test -d "${scriptdir}/../debian.${distribution}"; then
    tar cf - -C "${scriptdir}/../debian.${distribution}" . | tar xf - -C "${packagedir}/debian"
fi

pushd "${packagedir}"
for p in debian/post-patches/*.patch; do
    if test -f "${p}"; then
        echo "Applying ${p}"
        patch -p1 < "${p}"
    fi
done
popd

"${scriptdir}/git2changelog.py"  /tmp/git2changelog "${distribution}"
mv "${packagedir}/debian/changelog" "${packagedir}/debian/changelog.old"
cat /tmp/git2changelog "${packagedir}/debian/changelog.old" > "${packagedir}/debian/changelog"
