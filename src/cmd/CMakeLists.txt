set(cmd_NAME ${APPLICATION_EXECUTABLE}cmd)
set(cmd_SRC
    cmd.cpp
    simplesslerrorhandler.cpp
    netrcparser.cpp
   )


if(NOT BUILD_LIBRARIES_ONLY)
    add_executable(${cmd_NAME}  ${cmd_SRC})
    ecm_mark_nongui_executable(${cmd_NAME})

    target_link_libraries(${cmd_NAME} "${csync_NAME}" "${synclib_NAME}" Qt5::Core Qt5::Network)

    # Need tokenizer for netrc parser
    target_include_directories(${cmd_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src/3rdparty/qtokenizer)

    if(APPLE)
      install(TARGETS ${cmd_NAME} RUNTIME DESTINATION "${KDE_INSTALL_BUNDLEDIR}/${APPLICATION_EXECUTABLE}.app/Contents/MacOS/")
    else()
      install(TARGETS ${cmd_NAME} ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
    endif()
endif()

