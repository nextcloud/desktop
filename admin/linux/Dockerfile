# This DockerFile is used to create the image used for Jenkins, the CI system (see Jenkinsfile)
# It is not meant to be used to create the production packages.

# Distro with Qt 5.6
FROM ubuntu:yakkety

RUN apt-get update -q && DEBIAN_FRONTEND=noninteractive apt-get install -q -y --no-install-recommends \
        locales \
        build-essential \
        clang \
        ninja-build \
        cmake \
        extra-cmake-modules \
        libsqlite3-dev \
        libssl-dev \
        libcmocka-dev \
        qt5-default \
        qttools5-dev-tools \
        libqt5webkit5-dev \
        qt5keychain-dev \
        kio-dev \
    && apt-get clean
