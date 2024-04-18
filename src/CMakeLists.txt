include(ECMEnableSanitizers)

set(REQUIRED_QT_VERSION "5.15.0")

find_package(Qt5Core ${REQUIRED_QT_VERSION} CONFIG QUIET)
set_package_properties(Qt5Core PROPERTIES
    DESCRIPTION "Qt5 Core component."
    TYPE REQUIRED
)

find_package(Qt5Network ${REQUIRED_QT_VERSION} CONFIG QUIET)
set_package_properties(Qt5Network PROPERTIES
    DESCRIPTION "Qt5 Network component."
    TYPE REQUIRED
)

find_package(Qt5Xml ${REQUIRED_QT_VERSION} CONFIG QUIET)
set_package_properties(Qt5Xml PROPERTIES
    DESCRIPTION "Qt5 Xml component."
    TYPE REQUIRED
)

find_package(Qt5Concurrent ${REQUIRED_QT_VERSION} CONFIG QUIET)
set_package_properties(Qt5Concurrent PROPERTIES
    DESCRIPTION "Qt5 Concurrent component."
    TYPE REQUIRED
)

find_package(Qt5QuickWidgets ${REQUIRED_QT_VERSION} CONFIG QUIET)
set_package_properties(Qt5QuickWidgets PROPERTIES
    DESCRIPTION "Qt5 QuickWidgets component."
    TYPE REQUIRED
)

find_package(Qt5WebEngineWidgets ${REQUIRED_QT_VERSION} CONFIG QUIET)
if(NOT BUILD_WITH_WEBENGINE)
    set_package_properties(Qt5WebEngineWidgets PROPERTIES
        DESCRIPTION "Qt5 WebEngineWidgets component."
        TYPE RECOMMENDED
    )
else()
    set_package_properties(Qt5WebEngineWidgets PROPERTIES
        DESCRIPTION "Qt5 WebEngineWidgets component."
        TYPE REQUIRED
    )
endif()

find_package(Qt5WebEngine ${REQUIRED_QT_VERSION} CONFIG QUIET)
if(NOT BUILD_WITH_WEBENGINE)
    set_package_properties(Qt5WebEngine PROPERTIES
        DESCRIPTION "Qt5 WebEngine component."
        TYPE RECOMMENDED
    )
else()
    set_package_properties(Qt5WebEngine PROPERTIES
        DESCRIPTION "Qt5 WebEngine component."
        TYPE REQUIRED
    )
endif()

if(BUILD_WITH_WEBENGINE AND Qt5WebEngine_FOUND AND Qt5WebEngineWidgets_FOUND)
  add_compile_definitions(WITH_WEBENGINE=1)
endif()

get_target_property (QT_QMAKE_EXECUTABLE Qt5::qmake IMPORTED_LOCATION)
message(STATUS "Using Qt ${Qt5Core_VERSION} (${QT_QMAKE_EXECUTABLE})")

if(NOT TOKEN_AUTH_ONLY)
    find_package(Qt5Keychain REQUIRED)
endif()

# TODO: Mingw64 7.3 might also need to be excluded here as it seems to not automatically link libssp
if(NOT MSVC)
  if(NOT (CMAKE_SYSTEM_PROCESSOR MATCHES "^(alpha|parisc|hppa)") AND NOT CMAKE_CROSSCOMPILING)
    if((CMAKE_CXX_COMPILER_ID MATCHES "GNU") AND (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.9))
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fstack-protector --param=ssp-buffer-size=4")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fstack-protector --param=ssp-buffer-size=4")
    else()
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fstack-protector-strong")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fstack-protector-strong")
    endif()
  endif()

  string(TOLOWER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE_LOWER)
  if(CMAKE_BUILD_TYPE_LOWER MATCHES "(release|relwithdebinfo|minsizerel)" AND ((NOT ${CMAKE_C_FLAGS} MATCHES "FORTIFY_SOURCE=[3-9]") AND (NOT ${CMAKE_CXX_FLAGS} MATCHES "FORTIFY_SOURCE=[3-9]")))
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_FORTIFY_SOURCE=2")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_FORTIFY_SOURCE=2")
  endif()

  if (CMAKE_CXX_COMPILER MATCHES "Clang")
    # Calling Qt's qCWarning(category, ...) with no params for "..." is a GNU
    # extension (C++11 §16.3/4 forbids them). Silence clang's warnings.
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-gnu-zero-variadic-macro-arguments")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-gnu-zero-variadic-macro-arguments")
  endif()
endif()

if(WIN32)
  # Enable DEP, ASLR and CFG
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /nxcompat /dynamicbase /guard:cf")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /nxcompat /dynamicbase /guard:cf")

  # x86 only: Enable SafeSEH
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "i686.*|i386.*|x86.*")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /safeseh")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /safeseh")
  endif()
elseif(UNIX AND NOT APPLE)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-z,relro -Wl,-z,now")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-z,relro -Wl,-z,now")
elseif(APPLE)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-ld_classic")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-ld_classic")
endif()

set(QML_IMPORT_PATH ${CMAKE_SOURCE_DIR}/theme CACHE STRING "" FORCE)

add_subdirectory(csync)
add_subdirectory(libsync)
if (NOT BUILD_LIBRARIES_ONLY)
    add_subdirectory(cmd)
    if (BUILD_GUI)
        add_subdirectory(gui)
    endif()

    if (WITH_CRASHREPORTER)
        add_subdirectory(3rdparty/libcrashreporter-qt)
        add_subdirectory(crashreporter)
    endif()
endif(NOT BUILD_LIBRARIES_ONLY)
