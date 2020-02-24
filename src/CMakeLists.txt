# TODO: OSX and LIB_ONLY seem to require this to go to binary dir only
if(NOT TOKEN_AUTH_ONLY)
endif()

set(synclib_NAME ${APPLICATION_EXECUTABLE}sync)

find_package(Qt5 5.12 COMPONENTS Core Network Xml Concurrent REQUIRED)

if(NOT NO_WEBENGINE)
    find_package(Qt5 5.12 COMPONENTS WebEngineWidgets WebEngine REQUIRED)
endif()

if(NOT TOKEN_AUTH_ONLY)
    find_package(Qt5Keychain REQUIRED)
endif()

if(NOT MSVC)
  if(NOT (CMAKE_SYSTEM_PROCESSOR MATCHES "^(alpha|parisc|hppa)"))
    if((CMAKE_CXX_COMPILER_ID MATCHES "GNU") AND (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.9))
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fstack-protector --param=ssp-buffer-size=4")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fstack-protector --param=ssp-buffer-size=4")
    else()
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fstack-protector-strong")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fstack-protector-strong")
    endif()
  endif()

  string(TOLOWER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE_LOWER)
  if(CMAKE_BUILD_TYPE_LOWER MATCHES "(release|relwithdebinfo|minsizerel)")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_FORTIFY_SOURCE=2")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_FORTIFY_SOURCE=2")
  endif()

  # Calling Qt's qCWarning(category, ...) with no params for "..." is a GNU
  # extension (C++11 §16.3/4 forbids them). Silence clang's warnings.
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-gnu-zero-variadic-macro-arguments")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-gnu-zero-variadic-macro-arguments")
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
endif()

include_directories(
    ${CMAKE_SOURCE_DIR}/src/3rdparty
)

set(QML_IMPORT_PATH ${CMAKE_SOURCE_DIR}/theme CACHE STRING "" FORCE)

add_subdirectory(csync)
add_subdirectory(libsync)
if (NOT BUILD_LIBRARIES_ONLY)
    add_subdirectory(cmd)
    add_subdirectory(gui)

    if (WITH_CRASHREPORTER)
        add_subdirectory(3rdparty/libcrashreporter-qt)
        add_subdirectory(crashreporter)
    endif()
endif(NOT BUILD_LIBRARIES_ONLY)

find_program(KRAZY2_EXECUTABLE krazy2)
if(KRAZY2_EXECUTABLE)
    # s/y k/y ALL k/ for building this target always
    add_custom_target( krazy krazy2 --check-sets c++,qt4,foss
                       ${PROJECT_SOURCE_DIR}/src/libsync/*.ui
                       ${PROJECT_SOURCE_DIR}/src/libsync/*.h
                       ${PROJECT_SOURCE_DIR}/src/libsync/*.cpp
                       ${PROJECT_SOURCE_DIR}/src/gui/*.ui
                       ${PROJECT_SOURCE_DIR}/src/gui/*.h
                       ${PROJECT_SOURCE_DIR}/src/gui/*.cpp
                       ${PROJECT_SOURCE_DIR}/src/cmd/*.h
                       ${PROJECT_SOURCE_DIR}/src/cmd/*.cpp
)
endif()
