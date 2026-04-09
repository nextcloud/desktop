set( APPLICATION_REV_DOMAIN "com.ionos.hidrivenext.desktopclient" )

option(LOCALBUILD "Local developer build" OFF)

if(LOCALBUILD)
    ## Only needed for local build
    message(STATUS "Building in LOCAL mode")
    
    set( APPLICATION_VIRTUALFILE_SUFFIX "${APPLICATION_SHORTNAME}_virtual" CACHE STRING "Virtual file suffix (not including the .)" FORCE)

    ## Windows Shell Extensions & MSI - IMPORTANT: Generate new GUIDs for custom builds with "guidgen" or "uuidgen"
    if(WIN32)
        # Context Menu
        set( WIN_SHELLEXT_CONTEXT_MENU_GUID      "{6B16FF7B-F242-4CE3-8FB9-F06EF127E0DC}" )

        # Overlays
        set( WIN_SHELLEXT_OVERLAY_GUID_ERROR     "{243D887B-9F74-41DD-BACA-BC5501AF10AC}" )
        set( WIN_SHELLEXT_OVERLAY_GUID_OK        "{2D88D499-3272-4A76-84BF-D252254B40D6}" )
        set( WIN_SHELLEXT_OVERLAY_GUID_OK_SHARED "{7BEF6B56-5B5B-4284-A70C-56D62254C97A}" )
        set( WIN_SHELLEXT_OVERLAY_GUID_SYNC      "{5F2F493D-A683-426F-925E-4CA25F17C4A9}" )
        set( WIN_SHELLEXT_OVERLAY_GUID_WARNING   "{7F256BB6-29D2-4E40-A6C4-E5E756E64C82}" )

        # MSI Upgrade Code (without brackets)
        set( WIN_MSI_UPGRADE_CODE                "6C9E5670-E8A9-4BBD-9BDF-D003794AC177" )
    endif()

    if("${WHITELABEL_NAME}" STREQUAL "strato")
        set( APPLICATION_NAME       "STRATO HiDrive Next" )
        set( APPLICATION_SHORTNAME  "STRATOHiDriveNext" )
        set( APPLICATION_EXECUTABLE "strato-hidrive-next" )
        set( APPLICATION_CONFIG_NAME "STRATO-HiDrive-Next" )
        set( APPLICATION_ICON_NAME  "strato_hidrive_next" )
        set( APPLICATION_DOMAIN     "strato.com" )
        set( APPLICATION_UPDATE_URL "https://customerupdates.nextcloud.com/client/" CACHE STRING "URL for updater" FORCE)
        set( APPLICATION_HELP_URL   "" CACHE STRING "URL for the help menu" FORCE)
        set( APPLICATION_SERVER_URL "https://storage.ionos.fr" CACHE STRING "URL for the server to use. If entered, the UI field will be pre-filled with it" FORCE)
    elseif("${WHITELABEL_NAME}" STREQUAL "ionos")
        set( APPLICATION_NAME       "IONOS HiDrive Next" )
        set( APPLICATION_SHORTNAME  "IONOSHiDriveNext" )
        set( APPLICATION_EXECUTABLE "ionos-hidrive-next" )
        set( APPLICATION_CONFIG_NAME "IONOS-HiDrive-Next" )
        set( APPLICATION_ICON_NAME  "ionos_hidrive_next" )
        set( APPLICATION_DOMAIN     "ionos.com" )
        set( APPLICATION_UPDATE_URL "https://customerupdates.nextcloud.com/client/" CACHE STRING "URL for updater" FORCE)
        set( APPLICATION_HELP_URL   "" CACHE STRING "URL for the help menu" FORCE)
        set( APPLICATION_SERVER_URL "https://storage.ionos.fr" CACHE STRING "URL for the server to use. If entered, the UI field will be pre-filled with it" FORCE)
    endif()

endif()


if(APPLE AND "${APPLICATION_NAME}" MATCHES "HiDrive Next")
    set(APPLICATION_ICON_NAME "${APPLICATION_EXECUTABLE}-macOS")
    message("Using macOS-specific application icon: ${APPLICATION_ICON_NAME}")
endif()

if(APPLICATION_NAME STREQUAL "STRATO HiDrive Next")
    set( APPLICATION_VENDOR     "STRATO" )
    add_compile_definitions(STRATO_WL_BUILD)
elseif(APPLICATION_NAME STREQUAL "IONOS HiDrive Next")
    set( APPLICATION_VENDOR     "IONOS SE" )
    add_compile_definitions(IONOS_WL_BUILD)
endif()