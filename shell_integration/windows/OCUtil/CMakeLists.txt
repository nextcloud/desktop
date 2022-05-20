add_library(OCUtil STATIC
    CommunicationSocket.cpp
    RemotePathChecker.cpp
    StringUtil.cpp
    OCUtil.rc
 )

target_include_directories(OCUtil
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
)
