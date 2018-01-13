SET(MINGW_PREFIX  "i686-w64-mingw32")

# this one is important
SET(CMAKE_SYSTEM_NAME Windows)


# specify the cross compiler
SET(CMAKE_C_COMPILER ${MINGW_PREFIX}-gcc)
SET(CMAKE_CXX_COMPILER ${MINGW_PREFIX}-g++)
SET(CMAKE_RC_COMPILER ${MINGW_PREFIX}-windres)

# where is the target environment containing libraries
SET(CMAKE_FIND_ROOT_PATH  /usr/${MINGW_PREFIX}/sys-root/mingw)
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY  ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE  ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM  NEVER)


## configure qt variables
# generic
SET(QMAKESPEC               win32-g++-cross)

# dirs
SET(QT_LIBRARY_DIR          /usr/${MINGW_PREFIX}/bin)
SET(QT_PLUGINS_DIR          ${CMAKE_FIND_ROOT_PATH}/lib/qt5/plugins)
SET(QT_MKSPECS_DIR          ${CMAKE_FIND_ROOT_PATH}/share/qt5/mkspecs)
SET(QT_QT_INCLUDE_DIR       ${CMAKE_FIND_ROOT_PATH}/include)

# qt tools
SET(QT_QMAKE_EXECUTABLE         ${MINGW_PREFIX}-qmake-qt5)
SET(QT_MOC_EXECUTABLE           ${MINGW_PREFIX}-moc-qt5)
SET(QT_RCC_EXECUTABLE           ${MINGW_PREFIX}-rcc-qt5)
SET(Qt5Widgets_UIC_EXECUTABLE   ${MINGW_PREFIX}-uic-qt5)
SET(QT_LRELEASE_EXECUTABLE      ${MINGW_PREFIX}-lrelease-qt5)
