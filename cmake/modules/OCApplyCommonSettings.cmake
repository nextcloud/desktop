include(OCRcVersion)

# common target settings that are used by all important targets
# not adding strict compile settings
# included in apply_common_target_settings
function(apply_common_target_settings_soft targetName)
    if(FORCE_ASSERTS)
        target_compile_definitions(${targetName}
            PRIVATE
                QT_FORCE_ASSERTS
                )
    endif()
endfunction()

# common target settings that are used by all important targets
function(apply_common_target_settings targetName)
    apply_common_target_settings_soft(${targetName})
    add_windows_version_info(${targetName})
    target_compile_definitions(${targetName}
        PRIVATE
            QT_NO_CAST_TO_ASCII
            QT_NO_CAST_FROM_ASCII
            QT_NO_URL_CAST_FROM_STRING
            QT_NO_CAST_FROM_BYTEARRAY
            QT_USE_QSTRINGBUILDER
            QT_MESSAGELOGCONTEXT  # enable function name and line number in debug output
            QT_NO_FOREACH
    )
endfunction()
