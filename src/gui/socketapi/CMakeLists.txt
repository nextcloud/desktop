target_sources(nextcloudCore PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/socketapi.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/socketuploadjob.cpp
)

if( APPLE )
    target_sources(nextcloudCore PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/socketapisocket_mac.mm)
endif()
