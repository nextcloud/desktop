target_sources(owncloudGui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/socketapi.cpp
    )
    
if( APPLE )
    target_sources(owncloudGui PRIVATE socketapisocket_mac.mm)
endif()
