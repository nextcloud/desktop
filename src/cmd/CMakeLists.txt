if(NOT BUILD_LIBRARIES_ONLY)
    add_executable(cmd
        cmd.cpp
        netrcparser.cpp
    )
    set_target_properties(cmd PROPERTIES OUTPUT_NAME "${APPLICATION_EXECUTABLE}cmd")
    ecm_mark_nongui_executable(cmd)

    target_link_libraries(cmd csync libsync Qt5::Core Qt5::Network)

    # Need tokenizer for netrc parser
    target_include_directories(cmd PRIVATE ${CMAKE_SOURCE_DIR}/src/3rdparty/qtokenizer)

    if(APPLE)
      install(TARGETS cmd RUNTIME DESTINATION "${KDE_INSTALL_BUNDLEDIR}/${APPLICATION_EXECUTABLE}.app/Contents/MacOS/")
    else()
      install(TARGETS cmd ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
    endif()
endif()

