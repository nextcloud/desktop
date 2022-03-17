target_sources(nextcloudCore PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/socketapi.h
    ${CMAKE_CURRENT_SOURCE_DIR}/socketapi.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/socketuploadjob.h
    ${CMAKE_CURRENT_SOURCE_DIR}/socketuploadjob.cpp
)

if( APPLE )
    target_sources(nextcloudCore PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/socketapi_mac.mm)
endif()
