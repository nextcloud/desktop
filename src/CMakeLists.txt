if(WITH_CRASHREPORTER)
    find_package(CrashReporterQt REQUIRED)
endif()

find_package(Qt6Keychain 0.13 REQUIRED)

# TODO: Mingw64 7.3 might also need to be excluded here as it seems to not automatically link libssp
if(NOT WIN32)
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
  if(CMAKE_BUILD_TYPE_LOWER MATCHES "(release|relwithdebinfo|minsizerel)")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_FORTIFY_SOURCE=2")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_FORTIFY_SOURCE=2")
  endif()
endif()

if(WIN32)
  # Enable DEP & ASLR
  if (MINGW)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--nxcompat -Wl,--dynamicbase")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--nxcompat -Wl,--dynamicbase")
  endif()
elseif(UNIX AND NOT APPLE)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-z,relro -Wl,-z,now")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-z,relro -Wl,-z,now")
endif()

add_subdirectory(resources)
add_subdirectory(csync)
add_subdirectory(libsync)
if (BUILD_GUI)
    add_subdirectory(gui)
endif()
add_subdirectory(cmd)

if (WITH_CRASHREPORTER)
    add_subdirectory(crashreporter)
endif()

add_subdirectory(plugins)

install(EXPORT ${APPLICATION_SHORTNAME}Config DESTINATION "${KDE_INSTALL_CMAKEPACKAGEDIR}/${APPLICATION_SHORTNAME}" NAMESPACE ownCloud::)

ecm_setup_version(PROJECT
    VARIABLE_PREFIX ${APPLICATION_SHORTNAME}
    PACKAGE_VERSION_FILE "${CMAKE_CURRENT_BINARY_DIR}/${APPLICATION_SHORTNAME}ConfigVersion.cmake"
    SOVERSION ${MIRALL_SOVERSION}
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${APPLICATION_SHORTNAME}ConfigVersion.cmake" DESTINATION "${KDE_INSTALL_CMAKEPACKAGEDIR}/${APPLICATION_SHORTNAME}")
