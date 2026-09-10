# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

function(_qml_window_metadata qml_file root_type_out has_close_shortcut_out exception_out)
    file(STRINGS "${qml_file}" qml_lines)
    set(root_type)
    set(has_close_shortcut FALSE)
    set(has_exception FALSE)
    set(in_shortcut FALSE)
    set(in_block_comment FALSE)

    foreach(qml_line IN LISTS qml_lines)
        string(STRIP "${qml_line}" qml_line)

        if(qml_line MATCHES "^//[ \t]*QML_WINDOW_CLOSE_EXCEPTION:[ \t]*(.+)$")
            set(has_exception TRUE)
        endif()

        if(in_block_comment)
            if(qml_line MATCHES "\\*/")
                set(in_block_comment FALSE)
            endif()
            continue()
        endif()

        if(qml_line MATCHES "^/\\*")
            if(NOT qml_line MATCHES "\\*/")
                set(in_block_comment TRUE)
            endif()
            continue()
        endif()

        if(qml_line MATCHES "^//" OR qml_line STREQUAL "")
            continue()
        endif()

        if(NOT root_type)
            if(qml_line MATCHES "^import([ \t]|$)" OR qml_line MATCHES "^pragma([ \t]|$)")
                continue()
            endif()

            if(qml_line MATCHES "^([A-Za-z_][A-Za-z0-9_.]*)[ \t]*\\{")
                set(root_type "${CMAKE_MATCH_1}")
            endif()
        endif()

        if(qml_line MATCHES "^Shortcut[ \t]*\\{")
            set(in_shortcut TRUE)
        endif()

        if(in_shortcut AND qml_line MATCHES "StandardKey[ \t]*\\.[ \t]*Close")
            set(has_close_shortcut TRUE)
        endif()

        if(in_shortcut AND qml_line MATCHES "^\\}")
            set(in_shortcut FALSE)
        endif()
    endforeach()

    set("${root_type_out}" "${root_type}" PARENT_SCOPE)
    set("${has_close_shortcut_out}" "${has_close_shortcut}" PARENT_SCOPE)
    set("${exception_out}" "${has_exception}" PARENT_SCOPE)
endfunction()

function(check_qml_window_close_shortcuts qml_root)
    if(CMAKE_SCRIPT_MODE_FILE)
        file(GLOB_RECURSE qml_files "${qml_root}/*.qml")
    else()
        file(GLOB_RECURSE qml_files CONFIGURE_DEPENDS "${qml_root}/*.qml")
    endif()

    foreach(qml_file IN LISTS qml_files)
        get_filename_component(qml_type "${qml_file}" NAME_WLE)
        set(qml_type_file_${qml_type} "${qml_file}")
    endforeach()

    set(window_errors)
    foreach(qml_file IN LISTS qml_files)
        _qml_window_metadata("${qml_file}" root_type has_close_shortcut has_exception)
        if(NOT root_type)
            continue()
        endif()

        set(ancestor_type "${root_type}")
        set(ancestor_files)
        set(window_base_file)
        set(window_base_has_close_shortcut FALSE)
        set(window_base_has_exception FALSE)

        while(ancestor_type)
            if(ancestor_type MATCHES "^(ApplicationWindow|Window)$")
                set(window_base_file "${qml_file}")
                set(window_base_has_close_shortcut "${has_close_shortcut}")
                set(window_base_has_exception "${has_exception}")
                break()
            endif()

            list(FIND ancestor_files "${ancestor_type}" ancestor_seen)
            if(NOT ancestor_seen EQUAL -1)
                break()
            endif()
            list(APPEND ancestor_files "${ancestor_type}")

            set(type_variable "qml_type_file_${ancestor_type}")
            if(NOT DEFINED ${type_variable})
                break()
            endif()

            set(ancestor_file "${${type_variable}}")
            _qml_window_metadata("${ancestor_file}" ancestor_type ancestor_has_close_shortcut ancestor_has_exception)
            if(ancestor_type MATCHES "^(ApplicationWindow|Window)$")
                set(window_base_file "${ancestor_file}")
                set(window_base_has_close_shortcut "${ancestor_has_close_shortcut}")
                set(window_base_has_exception "${ancestor_has_exception}")
                break()
            endif()
        endwhile()

        if(window_base_file AND NOT window_base_has_close_shortcut AND NOT window_base_has_exception)
            list(APPEND window_errors
                "${qml_file}: window type inherits from ${window_base_file} without a Shortcut using StandardKey.Close")
        endif()
    endforeach()

    if(window_errors)
        list(JOIN window_errors "\n  - " formatted_errors)
        message(FATAL_ERROR
            "QML window close shortcut check failed:\n  - ${formatted_errors}\n"
            "Add StandardKey.Close or document a deliberate exception with "
            "QML_WINDOW_CLOSE_EXCEPTION: <reason>.")
    endif()
endfunction()

function(add_qml_window_close_shortcut_check qml_root consumer_target)
    if(NOT TARGET "${consumer_target}")
        message(FATAL_ERROR "Cannot attach the QML window close shortcut check to unknown target: ${consumer_target}")
    endif()

    check_qml_window_close_shortcuts("${qml_root}")
    file(GLOB_RECURSE qml_files CONFIGURE_DEPENDS "${qml_root}/*.qml")

    add_custom_target(check_qml_window_close_shortcuts_build
        COMMAND "${CMAKE_COMMAND}"
            "-DQML_WINDOW_CLOSE_SHORTCUTS_ROOT=${qml_root}"
            -P "${CMAKE_SOURCE_DIR}/cmake/modules/CheckQmlWindowCloseShortcuts.cmake"
        DEPENDS
            "${CMAKE_SOURCE_DIR}/cmake/modules/CheckQmlWindowCloseShortcuts.cmake"
            ${qml_files}
        COMMENT "Checking QML window close shortcuts"
        VERBATIM
    )
    add_dependencies("${consumer_target}" check_qml_window_close_shortcuts_build)
endfunction()

if(DEFINED QML_WINDOW_CLOSE_SHORTCUTS_ROOT)
    check_qml_window_close_shortcuts("${QML_WINDOW_CLOSE_SHORTCUTS_ROOT}")
endif()
