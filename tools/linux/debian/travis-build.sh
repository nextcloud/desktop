#!/bin/bash

set -xe
shopt -s extglob

TRAVIS_BUILD_STEP="$1"

PPA=ppa:nextcloud-devs/client-alpha
PPA_BETA=ppa:nextcloud-devs/client-beta

OBS_PROJECT=home:ivaradi:alpha
OBS_PROJECT_BETA=home:ivaradi:beta
OBS_PACKAGE=nextcloud-client

if [ "$TRAVIS_BUILD_STEP" == "install" ]; then
    sudo apt-get update -q
    sudo apt-get install -y devscripts cdbs osc

    if test "$encrypted_585e03da75ed_key" -a "$encrypted_585e03da75ed_iv"; then
        openssl aes-256-cbc -K $encrypted_585e03da75ed_key -iv $encrypted_585e03da75ed_iv -in tools/linux/debian/signing-key.txt.enc -d | gpg --import
        echo "DEBUILD_DPKG_BUILDPACKAGE_OPTS='-k7D14AA7B'" >> ~/.devscripts

        openssl aes-256-cbc -K $encrypted_585e03da75ed_key -iv $encrypted_585e03da75ed_iv -in tools/linux/debian/oscrc.enc -out ~/.oscrc -d
    elif test "$encrypted_8da7a4416c7a_key" -a "$encrypted_8da7a4416c7a_iv"; then
        openssl aes-256-cbc -K $encrypted_8da7a4416c7a_key -iv $encrypted_8da7a4416c7a_iv -in tools/linux/debian/oscrc.enc -out ~/.oscrc -d
        PPA=ppa:ivaradi/nextcloud-client-exp
    fi

elif [ "$TRAVIS_BUILD_STEP" == "script" ]; then
    read basever kind <<<$(tools/linux/debian/scripts/git2changelog.py /tmp/tmpchangelog stable)

    cd ..

    echo "$kind" > kind
    kind="release"

    if test "$kind" = "beta"; then
        repo=client-beta
    else
        repo=client-alpha
    fi

    if test -d nextcloud-client; then
        gitdir="nextcloud-client"
    else
        gitdir="client"
    fi

    origsourceopt=""

    ls -alR

    if ! wget http://ppa.launchpad.net/nextcloud-devs/${repo}/ubuntu/pool/main/n/nextcloud-client/nextcloud-client_${basever}.orig.tar.bz2; then
        mv ${gitdir} nextcloud-client_${basever}
        tar cjf nextcloud-client_${basever}.orig.tar.bz2 --exclude .git nextcloud-client_${basever}
        mv nextcloud-client_${basever} ${gitdir}
        origsourceopt="-sa"
    fi

    for distribution in xenial zesty artful stable; do
        rm -rf nextcloud-client_${basever}
        cp -a ${gitdir} nextcloud-client_${basever}

        cd nextcloud-client_${basever}

        cp -a tools/linux/debian/debian .
        if test -d tools/linux/debian/debian.${distribution}; then
            tar cf - -C tools/linux/debian/debian.${distribution} . | tar xf - -C debian
        fi

        tools/linux/debian/scripts/git2changelog.py /tmp/tmpchangelog ${distribution}
        cp /tmp/tmpchangelog debian/changelog
        if test -f tools/linux/debian/nextcloud-client/debian.${distribution}/changelog; then
            cat tools/linux/debian/nextcloud-client/debian.${distribution}/changelog >> debian/changelog
        else
            cat tools/linux/debian/nextcloud-client/debian/changelog >> debian/changelog
        fi

        EDITOR=true dpkg-source --commit . local-changes

        if test "$encrypted_585e03da75ed_key" -a "$encrypted_585e03da75ed_iv"; then
            debuild -S ${origsourceopt}
        else
            debuild -S ${origsourceopt} -us -uc
        fi

        cd ..
    done

elif [ "$TRAVIS_BUILD_STEP" == "ppa_deploy" ]; then
    cd ..

    kind=`cat kind`

    echo "kind: $kind"

    if test "$kind" = "beta"; then
        PPA=$PPA_BETA
        OBS_PROJECT=$OBS_PROJECT_BETA
    fi
    OBS_SUBDIR="${OBS_PROJECT}/${OBS_PACKAGE}"

    if test "$encrypted_585e03da75ed_key" -a "$encrypted_585e03da75ed_iv"; then
        for changes in nextcloud-client_*~+([a-z])1_source.changes; do
            dput $PPA $changes > /dev/null
        done
    fi

    mkdir osc
    cd osc
    osc co ${OBS_PROJECT} ${OBS_PACKAGE}
    if test "$(ls ${OBS_SUBDIR})"; then
        osc delete ${OBS_SUBDIR}/*
    fi
    cp ../nextcloud-client*.orig.tar.* ${OBS_SUBDIR}/
    cp ../nextcloud-client_*[0-9.][0-9].dsc ${OBS_SUBDIR}/
    cp ../nextcloud-client_*[0-9.][0-9].debian.tar* ${OBS_SUBDIR}/
    cp ../nextcloud-client_*[0-9.][0-9]_source.changes ${OBS_SUBDIR}/
    osc add ${OBS_SUBDIR}/*

    cd ${OBS_SUBDIR}
    osc commit -m "Travis update"
fi
