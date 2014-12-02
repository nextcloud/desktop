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
        # first convert image to a tiff using the Mac OS X "sips" utility,
        # then use tiff2icns to convert to an icon
        find_program(SIPS_EXECUTABLE NAMES sips)
        find_program(TIFF2ICNS_EXECUTABLE NAMES tiff2icns)
        if (SIPS_EXECUTABLE AND TIFF2ICNS_EXECUTABLE)
            file(GLOB_RECURSE files  "${pattern}")
            # we can only test for the 128-icon like that - we don't use patterns anymore
            foreach (it ${files})
                if (it MATCHES ".*128.*" )
                    set (_icon ${it})
                endif (it MATCHES ".*128.*")
            endforeach (it)

            if (_icon)
                
                # first, get the basename of our app icon
                add_custom_command(OUTPUT ${_outfilename}.icns ${outfilename}.tiff
                                   COMMAND ${SIPS_EXECUTABLE} -s format tiff ${_icon} --out ${outfilename}.tiff
                                   COMMAND ${TIFF2ICNS_EXECUTABLE} ${outfilename}.tiff ${_outfilename}.icns
                                   DEPENDS ${_icon}
                                   )

                # This will register the icon into the bundle
                set(MACOSX_BUNDLE_ICON_FILE ${appsources}.icns)

                # Append the icns file to the sources list so it will be a dependency to the
                # main target
                list(APPEND ${appsources} ${_outfilename}.icns)

                # Install the icon into the Resources dir in the bundle
                set_source_files_properties(${_outfilename}.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

            else(_icon)
                # TODO - try to scale a non-128 icon...? Try to convert an SVG on the fly?
                message(STATUS "Unable to find an 128x128 icon that matches pattern ${pattern} for variable ${appsources} - application will not have an application icon!")
            endif(_icon)

        else(SIPS_EXECUTABLE AND TIFF2ICNS_EXECUTABLE)
            message(STATUS "Unable to find the sips and tiff2icns utilities - application will not have an application icon!")
        endif(SIPS_EXECUTABLE AND TIFF2ICNS_EXECUTABLE)
    endif(APPLE)
endmacro (KDE4_ADD_APP_ICON)
