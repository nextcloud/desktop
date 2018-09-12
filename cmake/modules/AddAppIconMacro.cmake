SET(WINDRES_EXECUTABLE_BASE  ${CMAKE_RC_COMPILER})

# This macro is taken from kdelibs/cmake/modules/KDE4Macros.cmake.
#
# Copyright (c) 2006-2009 Alexander Neundorf, <neundorf@kde.org>
# Copyright (c) 2006, 2007, Laurent Montel, <montel@kde.org>
# Copyright (c) 2007 Matthias Kretz <kretz@kde.org>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file [in KDE repositories].


# adds application icon to target source list 
# for detailed documentation see the top of FindKDE4Internal.cmake
macro (KDE4_ADD_APP_ICON appsources pattern)
    set (_outfilename ${CMAKE_CURRENT_BINARY_DIR}/${appsources})

    if (WIN32)
        if(NOT WINCE)
        find_program(PNG2ICO_EXECUTABLE NAMES png2ico)
        else(NOT WINCE)
        find_program(PNG2ICO_EXECUTABLE NAMES png2ico PATHS ${HOST_BINDIR} NO_DEFAULT_PATH )
        endif(NOT WINCE)
        find_program(WINDRES_EXECUTABLE NAMES ${WINDRES_EXECUTABLE_BASE})
        if(MSVC)
            set(WINDRES_EXECUTABLE TRUE)
        endif(MSVC)
        if (PNG2ICO_EXECUTABLE AND WINDRES_EXECUTABLE)
	    string(REPLACE "*" "([0123456789]*)" pattern_rx "${pattern}")
            file(GLOB files  "${pattern}")
            foreach (it ${files})
                string(REGEX REPLACE "${pattern_rx}" "\\1" fn "${it}")
                if (fn MATCHES ".*16.*" )
                    list (APPEND _icons ${it})
                endif (fn MATCHES ".*16.*")
                if (fn MATCHES ".*32.*" )
                    list (APPEND _icons ${it})
                endif (fn MATCHES ".*32.*")
                if (fn MATCHES ".*48.*" )
                    list (APPEND _icons ${it})
                endif (fn MATCHES ".*48.*")
                if (fn MATCHES ".*64.*" )
                    list (APPEND _icons ${it})
                endif (fn MATCHES ".*64.*")
                if (fn MATCHES ".*128.*" )
                    list (APPEND _icons ${it})
                endif (fn MATCHES ".*128.*")
                if (fn MATCHES ".*256.*" )
                    list (APPEND _icons ${it})
                endif (fn MATCHES ".*256.*")                
            endforeach (it)
            if (_icons)
                add_custom_command(OUTPUT ${_outfilename}.ico ${_outfilename}.rc
                                   COMMAND ${PNG2ICO_EXECUTABLE} ARGS --rcfile ${_outfilename}.rc ${_outfilename}.ico ${_icons}
                                   DEPENDS ${PNG2ICO_EXECUTABLE} ${_icons}
                                   WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                                  )
                if (MINGW)
                    add_custom_command(OUTPUT ${_outfilename}_res.o
                                       COMMAND ${WINDRES_EXECUTABLE} ARGS -i ${_outfilename}.rc -o ${_outfilename}_res.o --include-dir=${CMAKE_CURRENT_SOURCE_DIR}
                                       DEPENDS ${WINDRES_EXECUTABLE} ${_outfilename}.rc
                                       WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                                      )
                    list(APPEND ${appsources} ${_outfilename}_res.o)
                else(MINGW)
                    list(APPEND ${appsources} ${_outfilename}.rc)
                endif(MINGW)
            else(_icons)
                message(STATUS "Unable to find a related icon that matches pattern ${pattern} for variable ${appsources} - application will not have an application icon!")
            endif(_icons)
        else(PNG2ICO_EXECUTABLE AND WINDRES_EXECUTABLE)
            message(WARNING "Unable to find the png2ico or windres utilities - application will not have an application icon!")
        endif(PNG2ICO_EXECUTABLE AND WINDRES_EXECUTABLE)
    endif(WIN32)
    if (APPLE)
        file(GLOB_RECURSE files "${pattern}")
        file(MAKE_DIRECTORY ${appsources}.iconset)

        # List from:
        # https://developer.apple.com/library/content/documentation/GraphicsAnimation/Conceptual/HighResolutionOSX/Optimizing/Optimizing.html#//apple_ref/doc/uid/TP40012302-CH7-SW4
        foreach (it ${files})
            if (it MATCHES ".*icon-16.*")
                configure_file(${it} ${appsources}.iconset/icon_16x16.png COPYONLY)
            elseif (it MATCHES ".*icon-32.*")
                configure_file(${it} ${appsources}.iconset/icon_16x16@2x.png COPYONLY)
                configure_file(${it} ${appsources}.iconset/icon_32x32.png COPYONLY)
            elseif (it MATCHES ".*icon-64.*")
                configure_file(${it} ${appsources}.iconset/icon_32x32@2x.png COPYONLY)
            elseif (it MATCHES ".*icon-128.*")
                configure_file(${it} ${appsources}.iconset/icon_128x128.png COPYONLY)
            elseif (it MATCHES ".*icon-256.*")
                configure_file(${it} ${appsources}.iconset/icon_128x128@2x.png COPYONLY)
                configure_file(${it} ${appsources}.iconset/icon_256x256.png COPYONLY)
            elseif (it MATCHES ".*icon-512.*")
                configure_file(${it} ${appsources}.iconset/icon_256x256@2x.png COPYONLY)
                configure_file(${it} ${appsources}.iconset/icon_512x512.png COPYONLY)
            elseif (it MATCHES ".*icon-1024.*")
                configure_file(${it} ${appsources}.iconset/icon_512x512@2x.png COPYONLY)
            endif()
        endforeach (it)

        # Copy the sidebar icons in the main app bundle for the FinderSync extension to pick.
        # https://developer.apple.com/library/content/documentation/General/Conceptual/ExtensibilityPG/Finder.html#//apple_ref/doc/uid/TP40014214-CH15-SW15
        foreach (it ${files})
            if (it MATCHES ".*sidebar-16.*")
                configure_file(${it} ${appsources}.iconset/sidebar_16x16.png COPYONLY)
            elseif (it MATCHES ".*sidebar-18.*")
                configure_file(${it} ${appsources}.iconset/sidebar_18x18.png COPYONLY)
            elseif (it MATCHES ".*sidebar-32.*")
                configure_file(${it} ${appsources}.iconset/sidebar_16x16@2x.png COPYONLY)
                configure_file(${it} ${appsources}.iconset/sidebar_32x32.png COPYONLY)
            elseif (it MATCHES ".*sidebar-36.*")
                configure_file(${it} ${appsources}.iconset/sidebar_18x18@2x.png COPYONLY)
            elseif (it MATCHES ".*sidebar-64.*")
                configure_file(${it} ${appsources}.iconset/sidebar_32x32@2x.png COPYONLY)
            endif()
        endforeach (it)

        add_custom_command(OUTPUT ${_outfilename}.icns
                           COMMAND echo === Building bundle icns with iconset:
                           COMMAND ls -1 ${appsources}.iconset
                           COMMAND iconutil -c icns -o ${_outfilename}.icns ${appsources}.iconset
                           DEPENDS ${files}
                           )

        # This will register the icon into the bundle
        set(MACOSX_BUNDLE_ICON_FILE ${appsources}.icns)

        # Append the icns file to the sources list so it will be a dependency to the
        # main target
        list(APPEND ${appsources} ${_outfilename}.icns)

        # Install the icon into the Resources dir in the bundle
        set_source_files_properties(${_outfilename}.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    endif(APPLE)
endmacro (KDE4_ADD_APP_ICON)
