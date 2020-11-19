#!/usr/bin/env bash

# Inspired by https://github.com/sparkle-project/Sparkle/issues/896#issuecomment-339904848

if [ "${#}" -ne 3 ]; then
  echo "$(basename ${0}):"
  echo
  echo "  This script can be used to verify the .tbz.sig file in releases during manual testing"
  echo "  It does the same verification as Sparkle but is itself *not* used by Sparkle."
  echo
  echo "  Usage: ${0} public_key_path update_archive_path base64_signature_content"
  echo
  exit 1
fi

set -euxo pipefail

# Make sure we are using system openssl and not one from brew
openssl() {
  /usr/bin/openssl "${@}"
}

openssl dgst -sha1 -binary < "${2}" | openssl dgst -sha1 -verify "${1}" -signature <(echo "${3}" | openssl enc -base64 -d)
