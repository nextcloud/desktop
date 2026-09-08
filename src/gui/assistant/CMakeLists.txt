# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

set(assistant_SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR})

set(assistant_SRCS
    ${assistant_SOURCE_DIR}/assistantapijob.h
    ${assistant_SOURCE_DIR}/assistantapijob.cpp
    ${assistant_SOURCE_DIR}/assistantclient.h
    ${assistant_SOURCE_DIR}/assistantclient.cpp
    ${assistant_SOURCE_DIR}/assistantcontroller.h
    ${assistant_SOURCE_DIR}/assistantcontroller.cpp
    ${assistant_SOURCE_DIR}/assistantconversationmodel.h
    ${assistant_SOURCE_DIR}/assistantconversationmodel.cpp
    ${assistant_SOURCE_DIR}/assistantmessagemodel.h
    ${assistant_SOURCE_DIR}/assistantmessagemodel.cpp
    ${assistant_SOURCE_DIR}/assistantmodule.h
    ${assistant_SOURCE_DIR}/assistantmodule.cpp
    ${assistant_SOURCE_DIR}/assistanttaskmodel.h
    ${assistant_SOURCE_DIR}/assistanttaskmodel.cpp
    ${assistant_SOURCE_DIR}/assistanttasktypemodel.h
    ${assistant_SOURCE_DIR}/assistanttasktypemodel.cpp
    ${assistant_SOURCE_DIR}/assistantutils.h
    ${assistant_SOURCE_DIR}/assistantutils.cpp
    ${assistant_SOURCE_DIR}/ocsassistantconnector.h
    ${assistant_SOURCE_DIR}/ocsassistantconnector.cpp
)

set(assistant_QRC ${assistant_SOURCE_DIR}/assistant.qrc)
