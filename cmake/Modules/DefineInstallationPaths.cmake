if (UNIX)
  IF (NOT APPLICATION_NAME)
    MESSAGE(STATUS "${PROJECT_NAME} is used as APPLICATION_NAME")
    SET(APPLICATION_NAME ${PROJECT_NAME})
  ENDIF (NOT APPLICATION_NAME)

  # Suffix for Linux
  SET(LIB_SUFFIX
    CACHE STRING "Define suffix of directory name (32/64)"
  )

  SET(EXEC_INSTALL_PREFIX
    "${CMAKE_INSTALL_PREFIX}"
    CACHE PATH  "Base directory for executables and libraries"
    FORCE
  )
  SET(SHARE_INSTALL_PREFIX
    "${CMAKE_INSTALL_PREFIX}/share"
    CACHE PATH "Base directory for files which go to share/"
    FORCE
  )
  SET(DATA_INSTALL_PREFIX
    "${SHARE_INSTALL_PREFIX}/${APPLICATION_NAME}"
    CACHE PATH "The parent directory where applications can install their data" FORCE)

  # The following are directories where stuff will be installed to
  SET(BIN_INSTALL_DIR
    "${EXEC_INSTALL_PREFIX}/bin"
    CACHE PATH "The ${APPLICATION_NAME} binary install dir (default prefix/bin)"
    FORCE
  )
  SET(SBIN_INSTALL_DIR
    "${EXEC_INSTALL_PREFIX}/sbin"
    CACHE PATH "The ${APPLICATION_NAME} sbin install dir (default prefix/sbin)"
    FORCE
  )
  SET(LIB_INSTALL_DIR
    "${EXEC_INSTALL_PREFIX}/lib${LIB_SUFFIX}"
    CACHE PATH "The subdirectory relative to the install prefix where libraries will be installed (default is prefix/lib)"
    FORCE
  )
  SET(LIBEXEC_INSTALL_DIR
    "${EXEC_INSTALL_PREFIX}/libexec"
    CACHE PATH "The subdirectory relative to the install prefix where libraries will be installed (default is prefix/libexec)"
    FORCE
  )
  SET(PLUGIN_INSTALL_DIR
    "${LIB_INSTALL_DIR}/${APPLICATION_NAME}"
    CACHE PATH "The subdirectory relative to the install prefix where plugins will be installed (default is prefix/lib/${APPLICATION_NAME})"
    FORCE
  )
  SET(INCLUDE_INSTALL_DIR
    "${CMAKE_INSTALL_PREFIX}/include"
    CACHE PATH "The subdirectory to the header prefix (default prefix/include)"
    FORCE
  )

  SET(DATA_INSTALL_DIR
    "${DATA_INSTALL_PREFIX}"
    CACHE PATH "The parent directory where applications can install their data (default prefix/share/${APPLICATION_NAME})"
    FORCE
  )
  SET(HTML_INSTALL_DIR
    "${DATA_INSTALL_PREFIX}/doc/HTML"
    CACHE PATH "The HTML install dir for documentation (default data/doc/html)"
    FORCE
  )
  SET(ICON_INSTALL_DIR
    "${DATA_INSTALL_PREFIX}/icons"
    CACHE PATH "The icon install dir (default data/icons/)"
    FORCE
  )
  SET(SOUND_INSTALL_DIR
    "${DATA_INSTALL_PREFIX}/sounds"
    CACHE PATH "The install dir for sound files (default data/sounds)"
    FORCE
  )

  SET(LOCALE_INSTALL_DIR
    "${SHARE_INSTALL_PREFIX}/locale"
    CACHE PATH "The install dir for translations (default prefix/share/locale)"
    FORCE
  )

  SET(XDG_APPS_DIR
    "${SHARE_INSTALL_PREFIX}/applications/"
    CACHE PATH "The XDG apps dir"
    FORCE
  )
  SET(XDG_DIRECTORY_DIR
    "${SHARE_INSTALL_PREFIX}/desktop-directories"
    CACHE PATH "The XDG directory"
    FORCE
  )

  SET(SYSCONF_INSTALL_DIR
    "${EXEC_INSTALL_PREFIX}/etc"
    CACHE PATH "The ${APPLICATION_NAME} sysconfig install dir (default prefix/etc)"
    FORCE
  )
  SET(MAN_INSTALL_DIR
    "${SHARE_INSTALL_PREFIX}/man"
    CACHE PATH "The ${APPLICATION_NAME} man install dir (default prefix/man)"
    FORCE
  )
  SET(INFO_INSTALL_DIR
    "${SHARE_INSTALL_PREFIX}/info"
    CACHE PATH "The ${APPLICATION_NAME} info install dir (default prefix/info)"
    FORCE
  )
endif (UNIX)

if (WIN32)
  # Same same
  SET(BIN_INSTALL_DIR .)
  SET(SBIN_INSTALL_DIR .)
  SET(LIB_INSTALL_DIR .)
  SET(PLUGIN_INSTALL_DIR plugins)
  SET(HTML_INSTALL_DIR doc/HTML)
  SET(ICON_INSTALL_DIR .)
  SET(SOUND_INSTALL_DIR .)
  SET(LOCALE_INSTALL_DIR lang)
endif (WIN32)

