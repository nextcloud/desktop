#!/bin/bash

## log stdout stderr and set -x to a file
# exec  > "~/@APPLICATION_EXECUTABLE@-pre-install.log"
# exec  2>&1
# BASH_XTRACEFD=1
# set -x

# don't grep in one line, to avaoid grepping the grep process...
PROCESSES=$(ps aux)
OC_INSTANCE=$(echo "${PROCESSES}" | grep "/Applications/@APPLICATION_EXECUTABLE@.app/Contents/MacOS/@APPLICATION_EXECUTABLE@")

if [[ "${OC_INSTANCE}" != "" ]]; then
   kill $(echo "${OC_INSTANCE}" | awk '{print $2}')
   touch $INSTALLER_TEMP/OC_RESTART_NEEDED
fi

exit 0
