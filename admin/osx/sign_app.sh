#!/bin/sh -xe

[ "$#" -lt 2 ] && echo "Usage: sign_app.sh <app> <identity> <team_identifier>" && exit

src_app="$1"
identity="$2"
team_identifier="$3"

codesign -s "$identity" --force --preserve-metadata=entitlements --verbose=4 --deep "$src_app"

# Verify the signature
spctl -a -t exec -vv $src_app
codesign -dv $src_app

# Validate that the key used for signing the binary matches the expected TeamIdentifier
# needed to pass the SocketApi through the sandbox
codesign -dv $src_app 2>&1 | grep "TeamIdentifier=$team_identifier"
exit $?