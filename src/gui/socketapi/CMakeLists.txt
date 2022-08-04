target_sources(owncloudCore PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/socketapi.cpp
    )
    
if( APPLE )
    target_sources(owncloudCore PRIVATE socketapisocket_mac.mm)
endif()
