if (APPLE)
  find_program(HOMEBREW_EXECUTABLE brew)
  mark_as_advanced(FORCE HOMEBREW_EXECUTABLE)
  if (HOMEBREW_EXECUTABLE)
    # Detected a Homebrew install, query for its install prefix.
    execute_process(COMMAND ${HOMEBREW_EXECUTABLE} --prefix
      OUTPUT_VARIABLE HOMEBREW_INSTALL_PREFIX
      OUTPUT_STRIP_TRAILING_WHITESPACE)
    message(STATUS "Detected Homebrew with install prefix: "
      "${HOMEBREW_INSTALL_PREFIX}, adding to CMake search paths.")
    list(APPEND CMAKE_PREFIX_PATH "${HOMEBREW_INSTALL_PREFIX}")
    
    # Use Homebrew to find Qt.
    execute_process(COMMAND ${HOMEBREW_EXECUTABLE} --prefix qt
      OUTPUT_VARIABLE QT_PREFIX
      OUTPUT_STRIP_TRAILING_WHITESPACE)
      set(QT_PATH "${QT_PREFIX}/bin")
      message(STATUS "QT_PATH detected as ${QT_PATH}")
      set(Qt5_DIR "${QT_PREFIX}/lib/cmake/Qt5")
      message(STATUS "Qt5_DIR detected as ${Qt5_DIR}")
      set(Qt5LinguistTools_DIR "${QT_PREFIX}/lib/cmake/Qt5LinguistTools/")
      message(STATUS "Qt5LinguistTools_DIR detected as ${Qt5LinguistTools_DIR}")
      
  endif()
endif()
