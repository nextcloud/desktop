#!/usr/bin/env bash

# $1 - template directory
#
# Generates a template file for notification

COMMIT_SHA_SHORT=${DRONE_COMMIT:0:8}
SERVERS=("oc10" "ocis")
BUILD_STATUS=":white_check_mark:**Success**"
TEST_LOGS=""

if [ "${DRONE_BUILD_STATUS}" != "success" ]; then
    BUILD_STATUS=":x:**Failure**"

    for server in "${SERVERS[@]}"; do
        LOG_URL_PATH="${CACHE_ENDPOINT}/${CACHE_BUCKET}/${DRONE_REPO}/${DRONE_BUILD_NUMBER}/${server}/guiReportUpload"
        GUI_LOG="${LOG_URL_PATH}/index.html"
        SERVER_LOG="${LOG_URL_PATH}/serverlog.log"
        STACKTRACE="${LOG_URL_PATH}/stacktrace.log"

        CURL="curl --write-out %{http_code} --silent --output /dev/null"

        GUI_STATUS_CODE=$($CURL "$GUI_LOG")
        SERVER_STATUS_CODE=$($CURL "$SERVER_LOG")
        STACKTRACE_STATUS_CODE=$($CURL "$STACKTRACE")

        LOGS=""
        if [[ "$SERVER_STATUS_CODE" == "200" ]]; then
            LOGS+="\n> [Server log]($SERVER_LOG)  " # 2 spaces at the end act as line-break
        fi
        if [[ "$GUI_STATUS_CODE" == "200" ]]; then
            LOGS+="\n> [GUI test log]($GUI_LOG)"
        fi
        if [[ "$STACKTRACE_STATUS_CODE" == "200" ]]; then
            LOGS+="\n> [Stacktrace]($STACKTRACE)"
        fi

        if [[ -n "${LOGS}" ]]; then
            LOGS="\n${server}${LOGS}"
            TEST_LOGS+="${LOGS}"
        fi
    done
fi

echo -e "$BUILD_STATUS [${DRONE_REPO}#${COMMIT_SHA_SHORT}](${DRONE_BUILD_LINK}) (${DRONE_BRANCH}) by **${DRONE_COMMIT_AUTHOR}** $TEST_LOGS" >"$1"/template.md
