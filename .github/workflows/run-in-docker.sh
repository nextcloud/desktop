#! /bin/bash

image="$1"
shift

commands="$@"

if [[ "$image" == "" ]] || [[ "${commands[@]}" == "" ]]; then
    echo "Usage: bash $0 <image> <commands>"
    exit 1
fi

set -exo pipefail

extra_args=()

if tty -s; then
    extra_args+=("-t")
fi

docker run \
    --rm \
    --user "$(id -u)" \
    -i "${extra_args[@]}" \
    -e TX_TOKEN \
    -e HOME \
    -v "$HOME:$HOME" \
    -v "$(readlink -f .)":/ws \
    --entrypoint sh \
    -w /ws \
    "$image" \
    -c "${commands[@]}"
