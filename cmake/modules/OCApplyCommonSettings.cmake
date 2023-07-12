include(OCRcVersion)

if (MSVC)
    # ecm sets /W3 we set /W4
    string(REGEX REPLACE "/W3" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
endif()

# common target settings that are used by all important targets
function(apply_common_target_settings targetName)
    add_windows_version_info(${targetName})
    if(FORCE_ASSERTS)
        target_compile_definitions(${targetName}
            PRIVATE
                QT_FORCE_ASSERTS
        )
    endif()

    target_compile_definitions(${targetName}
        PRIVATE
            QT_NO_CAST_TO_ASCII
            QT_NO_CAST_FROM_ASCII
            QT_NO_URL_CAST_FROM_STRING
            QT_NO_CAST_FROM_BYTEARRAY
            QT_USE_QSTRINGBUILDER
            QT_MESSAGELOGCONTEXT  # enable function name and line number in debug output
            QT_NO_FOREACH
            QT_DISABLE_DEPRECATED_BEFORE=0x060200
    )

    if(WIN32)
        target_compile_definitions(${targetName}
            PRIVATE
                # Get APIs from from Win8 onwards.
                _WIN32_WINNT=_WIN32_WINNT_WIN8
                WINVER=_WIN32_WINNT_WIN8
                NTDDI_VERSION=NTDDI_WIN10_RS2
        )
    endif()

    if(MSVC)
        target_compile_options(${targetName}
            PRIVATE
                # enable linter like warnings with msvc
                # this includes deprecations
                # https://learn.microsoft.com/en-us/cpp/build/reference/compiler-option-warning-level?view=msvc-170
                /W4
                # treat unhandled switch cases as error
                /we4062
                # Worder
                /w15038
                # werror on unused function
                # The given function is local and not referenced in the body of the module; therefore, the function is dead code.
                /we4505
                # 4505 for anonymous namespaces, apparently undocumented https://developercommunity.visualstudio.com/t/warning-C4505-missing-on-anonymous-names/10413660
                /we5245
                /we4930
                # A variable is declared and initialized but not used.
                /we4189
        )
    elseif(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
        target_compile_options(${targetName}
            PRIVATE
                # treat unhandled switch cases as error
                -Werror=switch
                -Werror=unused-function
                -Werror=unused-but-set-variable
        )
    endif()
endfunction()
