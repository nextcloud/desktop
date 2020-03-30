#!/bin/bash

OC_INSTANCE=$(ps aux | grep "/Applications/@APPLICATION_EXECUTABLE@.app/Contents/MacOS/@APPLICATION_EXECUTABLE@")

if [[ "${OC_INSTANCE}" != "" ]]; then
   kill $(echo ${OC_INSTANCE} | awk '{print $2}')
   touch $INSTALLER_TEMP/OC_RESTART_NEEDED
fi

exit 0
