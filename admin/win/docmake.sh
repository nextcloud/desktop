# %_mingw32_qt4_platform          win32-g++-cross

export QT_BINDIR=/usr/bin
export BIN_PRE=i686-w64-mingw32

/usr/bin/mingw32-cmake \
        -DCMAKE_BUILD_TYPE="Debug" \
        -DQMAKESPEC=win32-g++-cross \
        -DQT_MKSPECS_DIR:PATH=/usr/i686-w64-mingw32/sys-root/mingw/share/qt4/mkspecs \
        -DQT_QT_INCLUDE_DIR=/usr/i686-w64-mingw32/sys-root/mingw/include \
        -DQT_PLUGINS_DIR=/usr/i686-w64-mingw32/sys-root/mingw/lib/qt4/plugins \
        -DQT_QMAKE_EXECUTABLE=${QT_BINDIR}/${BIN_PRE}-qmake \
        -DQT_MOC_EXECUTABLE=${QT_BINDIR}/${BIN_PRE}-moc \
        -DQT_RCC_EXECUTABLE=${QT_BINDIR}/${BIN_PRE}-rcc \
        -DQT_UIC_EXECUTABLE=${QT_BINDIR}/${BIN_PRE}-uic \
	-DQT_LRELEASE_EXECUTABLE=${QT_BINDIR}/${BIN_PRE}-lrelease \
        -DQT_DBUSXML2CPP_EXECUTABLE=${QT_BINDIR}/qdbusxml2cpp \
        -DQT_DBUSCPP2XML_EXECUTABLE=${QT_BINDIR}/qdbuscpp2xml ../../mirall 
	
