function(__add_file_to_qrc_file)
    set(options "")
    set(oneValueArgs QRC_PATH FILE_PATH ALIAS)
    set(multiValueArgs)
    cmake_parse_arguments(__ADD_FILE_TO_QRC_FILE "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    foreach(param QRC_PATH FILE_PATH)
        if(NOT __ADD_FILE_TO_QRC_FILE_${param})
            message(FATAL_ERROR "${CMAKE_CURRENT_FUNCTION}: Argument missing: ${param}")
        endif()
    endforeach()

    set(line "<file ")
    if(__ADD_FILE_TO_QRC_FILE_ALIAS)
        set(line "${line} alias=\"${__ADD_FILE_TO_QRC_FILE_ALIAS}\"")
    endif()
    set(line "${line}>${__ADD_FILE_TO_QRC_FILE_FILE_PATH}</file>")

    file(APPEND ${__ADD_FILE_TO_QRC_FILE_QRC_PATH} "        ${line}\n")
endfunction()


function(__addIcon QRC_PATH THEME ICON_NAME)
    set(options)
    set(oneValueArgs SRC_PATH)
    set(multiValueArgs)
    cmake_parse_arguments(_ICON "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT _ICON_SRC_PATH)
        set(_ICON_SRC_PATH ${THEME})
    endif()

    set(icon "theme/${_ICON_SRC_PATH}/${ICON_NAME}.svg")
    set(iconAlias "${APPLICATION_SHORTNAME}/theme/${THEME}/${ICON_NAME}.svg")
    if (EXISTS ${OEM_THEME_DIR}/${icon})
        file(APPEND "${QRC_PATH}" "<file alias=\"${iconAlias}\">${OEM_THEME_DIR}/${icon}</file>\n")
    else()
        set(icon "theme/${_ICON_SRC_PATH}/${ICON_NAME}.png")
        set(iconAlias "${APPLICATION_SHORTNAME}/theme/${THEME}/${ICON_NAME}.png")
        if (EXISTS ${OEM_THEME_DIR}/${icon})
            __add_file_to_qrc_file(
                QRC_PATH ${QRC_PATH}
                FILE_PATH ${OEM_THEME_DIR}/${icon}
                ALIAS ${iconAlias}
            )
        else()
            set(SIZES "16;22;32;48;64;128;256;512;1024")
            foreach(size ${SIZES})
                set(icon "theme/${_ICON_SRC_PATH}/${ICON_NAME}-${size}.png")
                set(iconAlias "${APPLICATION_SHORTNAME}/theme/${THEME}/${ICON_NAME}-${size}.png")
                if (EXISTS ${OEM_THEME_DIR}/${icon})
                    __add_file_to_qrc_file(
                        QRC_PATH ${QRC_PATH}
                        FILE_PATH ${OEM_THEME_DIR}/${icon}
                        ALIAS ${iconAlias}
                    )
                endif()
            endforeach()
        endif()
    endif()
endfunction()

function(__write_qrc_file_header QRC_PATH FILES_PREFIX)
    file(WRITE ${QRC_PATH} "<RCC>\n")
    file(APPEND ${QRC_PATH} "    <qresource prefix=\"/client/\">\n")
endfunction()

function(__write_qrc_file_footer QRC_PATH)
    file(APPEND ${QRC_PATH} "    </qresource>\n")
    file(APPEND ${QRC_PATH} "</RCC>\n")
endfunction()

function(generate_theme TARGET OWNCLOUD_SIDEBAR_ICONS_OUT)
    if(NOT "${OEM_THEME_DIR}" STREQUAL "${PROJECT_SOURCE_DIR}/src/resources/")
        set(QRC_PATH ${CMAKE_CURRENT_BINARY_DIR}/theme.qrc)
        __write_qrc_file_header(${QRC_PATH} theme)

        __addIcon(${QRC_PATH} "universal" "${APPLICATION_ICON_NAME}-icon" SRC_PATH "colored")
        __addIcon(${QRC_PATH} "universal" "wizard_logo" SRC_PATH "colored")

        set(STATES "ok;error;information;offline;pause;sync")
        set(THEMES "colored;dark;black;white")
        foreach(theme ${THEMES})
            foreach(state ${STATES})
                __addIcon(${QRC_PATH} ${theme} "state-${state}")
            endforeach()
        endforeach()

        __write_qrc_file_footer(${QRC_PATH})

        target_sources(${TARGET} PRIVATE ${QRC_PATH})
        target_compile_definitions(${TARGET} PRIVATE BRANDING_AVAILABLE)

        # add executable icon on windows and osx
        file(GLOB_RECURSE OWNCLOUD_SIDEBAR_ICONS "${OEM_THEME_DIR}/theme/colored/*-${APPLICATION_ICON_NAME}-sidebar.png")
    else()
        file(GLOB_RECURSE OWNCLOUD_SIDEBAR_ICONS "${OEM_THEME_DIR}/theme/colored/*-${APPLICATION_ICON_NAME}-icon-sidebar.png")
    endif()
    if (NOT OWNCLOUD_SIDEBAR_ICONS)
        message(WARNING "The branding does not provide sidebar icons falling back to vanilla icons")
        file(GLOB_RECURSE OWNCLOUD_SIDEBAR_ICONS "${PROJECT_SOURCE_DIR}/theme/colored/*-owncloud-icon-sidebar.png")
    endif()
    set(${OWNCLOUD_SIDEBAR_ICONS_OUT} ${OWNCLOUD_SIDEBAR_ICONS} PARENT_SCOPE)
endfunction()


function(generate_legacy_icons theme_dir OUT)
    # allow legacy file names
    file(GLOB_RECURSE OWNCLOUD_ICONS_OLD "${theme_dir}/colored/${APPLICATION_ICON_NAME}-icon-*.png")
    foreach(icon ${OWNCLOUD_ICONS_OLD})
        get_filename_component(icon_name ${icon} NAME)
        string(REGEX MATCH "([0-9]+)" size ${icon_name})
        set(out_name "${CMAKE_BINARY_DIR}/${size}-app-icon.png")
        configure_file(${icon} ${out_name} COPYONLY)
        list(APPEND OWNCLOUD_ICONS ${out_name})
    endforeach()
    set(${OUT} ${OWNCLOUD_ICONS} PARENT_SCOPE)
endfunction()

function(generate_qrc_file)
    set(options "")
    set(oneValueArgs QRC_PATH PREFIX)
    set(multiValueArgs FILES)
    cmake_parse_arguments(GENERATE_QRC_FILE "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    foreach(param ${oneValueArgs} ${multiValueArgs})
        if(NOT GENERATE_QRC_FILE_${param})
            message(FATAL_ERROR "${CMAKE_CURRENT_FUNCTION}: Argument missing: ${param}")
        endif()
    endforeach()

    __write_qrc_file_header(${GENERATE_QRC_FILE_QRC_PATH} ${GENERATE_QRC_FILE_PREFIX})

    foreach(file ${GENERATE_QRC_FILE_FILES})
        get_filename_component(file_name ${file} NAME)
        set(file_alias ${GENERATE_QRC_FILE_PREFIX}/${file_name})
        __add_file_to_qrc_file(
            QRC_PATH ${GENERATE_QRC_FILE_QRC_PATH}
            FILE_PATH ${file}
            ALIAS ${file_alias}
        )
    endforeach()

    __write_qrc_file_footer(${GENERATE_QRC_FILE_QRC_PATH})
endfunction()

# add resources to a target using the Qt resources system
# parameters:
#   - TARGET: the target to bundle the resources with
#   - PREFIX: virtual "subdirectory" the files will be available from
#   - FILES: the files to bundle
function(add_resources_to_target)
    set(options "")
    set(oneValueArgs TARGET PREFIX)
    set(multiValueArgs FILES)
    cmake_parse_arguments(ADD_RESOURCES_TO_TARGET "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    foreach(param ${oneValueArgs} ${multiValueArgs})
        if(NOT ADD_RESOURCES_TO_TARGET_${param})
            message(FATAL_ERROR "${CMAKE_CURRENT_FUNCTION}: Argument missing: ${param}")
        endif()
    endforeach()

    set(qrc_path ${CMAKE_CURRENT_BINARY_DIR}/${ADD_RESOURCES_TO_TARGET_TARGET}-${ADD_RESOURCES_TO_TARGET_PREFIX}.qrc)
    generate_qrc_file(
        QRC_PATH ${qrc_path}
        PREFIX ${ADD_RESOURCES_TO_TARGET_PREFIX}
        FILES "${ADD_RESOURCES_TO_TARGET_FILES}"
    )
    target_sources(${ADD_RESOURCES_TO_TARGET_TARGET} PRIVATE ${qrc_path})
endfunction()
