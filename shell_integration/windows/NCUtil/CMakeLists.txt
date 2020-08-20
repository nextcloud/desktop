add_library(NCUtil STATIC
    CommunicationSocket.cpp
    RemotePathChecker.cpp
    StringUtil.cpp
    NCUtil.rc
)

target_include_directories(NCUtil
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
)
