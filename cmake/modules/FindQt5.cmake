if (APPLE AND NOT QT_PREFIX)
  # Use Homebrew to find Qt.
  execute_process(COMMAND ${HOMEBREW_EXECUTABLE} --prefix qt
    OUTPUT_VARIABLE QT_PREFIX
    OUTPUT_STRIP_TRAILING_WHITESPACE)

  set(QT_PATH "${QT_PREFIX}/bin")
  # message(STATUS "QT_PATH detected as ${QT_PATH}")

  set(Qt5_DIR "${QT_PREFIX}/lib/cmake/Qt5")
  # message(STATUS "Qt5_DIR detected as ${Qt5_DIR}")

  set(Qt5LinguistTools_DIR "${QT_PREFIX}/lib/cmake/Qt5LinguistTools/")
  # message(STATUS "Qt5LinguistTools_DIR detected as ${Qt5LinguistTools_DIR}")

  find_package(Qt5
    HINTS
      "${Qt5_DIR}"
  )
endif()
