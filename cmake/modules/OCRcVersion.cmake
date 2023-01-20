set(_VERSION_SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR})

include(${PROJECT_SOURCE_DIR}/VERSION.cmake)
include(${PROJECT_SOURCE_DIR}/THEME.cmake)

function(add_windows_version_info targetName)
    if(NOT WIN32)
        return()
    endif()

    if(MIRALL_VERSION_BUILD)
        set(OC_RC_VERSION_BUILD ${MIRALL_VERSION_BUILD})
    else()
        set(OC_RC_VERSION_BUILD 0)
    endif()

    get_target_property(TARGET_TYPE ${targetName} TYPE)
    if(${TARGET_TYPE} STREQUAL "EXECUTABLE")
        set(OC_RC_TYPE "VFT_APP")
    elseif(${TARGET_TYPE} STREQUAL "SHARED_LIBRARY" OR ${TARGET_TYPE} STREQUAL "MODULE_LIBRARY")
        set(OC_RC_TYPE "VFT_DLL")
    else()
        # only create version.rc for dll's and executables
        return()
    endif()

    set(OC_RC_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${targetName}-oc_rc_version.rc)
    configure_file(${_VERSION_SOURCE_DIR}/version.rc.in ${OC_RC_OUTPUT} @ONLY)

    message(MESSAGE "TARGET_TYPE: ${TARGET_TYPE} ${OC_RC_OUTPUT}")
    target_sources(${targetName} PRIVATE ${OC_RC_OUTPUT})


endfunction()