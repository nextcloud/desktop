#!/usr/bin/env bash

# $1 - GUI_TEST_REPORT_DIR
# $2 - DRONE_REPO
# $3 - DRONE_BUILD_NUMBER
# $4 - server type ('oc10' or 'ocis')

REPORT_DIR="$1"
REPO="$2"
BUILD_NUMBER="$3"
SERVER_TYPE="$4"

LOG_URL_PATH="https://cache.owncloud.com/public/${REPO}/${BUILD_NUMBER}/${SERVER_TYPE}/guiReportUpload"
CURL="curl --write-out %{http_code} --silent --output /dev/null"

# $1 - log file path
# $2 - heading
function check_log() {
    if [[ -f "${REPORT_DIR}/$1" ]]; then
        LOG_FILE_URL="${LOG_URL_PATH}/$1"
        STATUS_CODE=$($CURL "$LOG_FILE_URL")
        if [[ "$STATUS_CODE" == "200" ]]; then
            echo -e "$2: $LOG_FILE_URL"
        fi
    fi
}

echo -e "Please, find the logs in the following links:\n"

# check gui report
check_log "index.html" "GUI Report"
# check server log
check_log "serverlog.log" "Client log"
# check stacktrace
check_log "stacktrace.log" "Stacktrace"

# check screenshots
if [[ -d "${REPORT_DIR}/screenshots" ]]; then
    echo -e "Screenshots:"
    for i in "${REPORT_DIR}"/screenshots/*.png; do
        echo -e "\t - ${LOG_URL_PATH}/screenshots/$(basename "$i")"
    done
fi
