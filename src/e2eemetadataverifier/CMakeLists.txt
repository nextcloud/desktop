# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

project(e2eemetadataverifier)

set(CMAKE_AUTOMOC TRUE)

if(UNIX AND NOT APPLE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIE")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIE")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pie")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L/usr/local/lib")
endif()

if(NOT BUILD_LIBRARIES_ONLY)
    add_executable(nextcloude2eemetadataverifier
        e2eemetadataverifier.h
        e2eemetadataverifier.cpp
        main.cpp)
    ecm_mark_nongui_executable(nextcloude2eemetadataverifier)
    set_target_properties(nextcloude2eemetadataverifier PROPERTIES
        RUNTIME_OUTPUT_NAME "${APPLICATION_EXECUTABLE}e2eemetadataverifier")

    target_link_libraries(nextcloude2eemetadataverifier
        Qt::Core
        Qt::Network)

    target_compile_definitions(nextcloude2eemetadataverifier PRIVATE
        APPLICATION_EXECUTABLE="${APPLICATION_EXECUTABLE}")

    if(BUILD_OWNCLOUD_OSX_BUNDLE)
        set_target_properties(nextcloude2eemetadataverifier PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${BIN_OUTPUT_DIRECTORY}/${OWNCLOUD_OSX_BUNDLE}/Contents/MacOS")
    else()
        set_target_properties(nextcloude2eemetadataverifier PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIRECTORY})

        install(TARGETS nextcloude2eemetadataverifier
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

        if(WIN32)
            install(FILES $<TARGET_PDB_FILE:nextcloude2eemetadataverifier> DESTINATION bin OPTIONAL)
        endif()
    endif()
endif()
