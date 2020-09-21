# Find WiX Toolset
if(NOT DEFINED ENV{WIX})
    # Example: WIX=C:\Program Files (x86)\WiX Toolset v3.11\
    message(FATAL_ERROR "WiX Toolset path not set (environment variable 'WIX'). Please install the WiX Toolset.")
else()
    set(WIX_SDK_PATH $ENV{WIX}/SDK/VS2017)
    message(STATUS "WiX Toolset SDK path: ${WIX_SDK_PATH}")
endif()

include_directories(${WIX_SDK_PATH}/inc)

if(CMAKE_SIZEOF_VOID_P MATCHES 4)
    link_directories(${WIX_SDK_PATH}/lib/x86)
else()
    link_directories(${WIX_SDK_PATH}/lib/x64)
endif()

add_definitions(-D_NCMSIHELPER_EXPORTS)
add_definitions(-D_USRDLL)
add_definitions(-D_WINDLL)

set(TARGET_NAME NCMsiHelper${BITNESS})

add_library(${TARGET_NAME} MODULE
    CustomAction.cpp
    CustomAction.def
    LogResult.cpp
    NCMsiHelper.cpp
)

target_link_libraries(${TARGET_NAME}
    NCToolsShared
)

install(TARGETS ${TARGET_NAME}
    DESTINATION msi/
)
install(FILES
    NCMsiHelper.wxs
    DESTINATION msi/
)
