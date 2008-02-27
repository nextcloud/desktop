# define system dependent compiler flags

include(CheckCXXCompilerFlag)

if (UNIX AND NOT WIN32)
  # with -fPIC
  if (CMAKE_SIZEOF_VOID_P MATCHES "8")
    check_cxx_compiler_flag("-fPIC" WITH_FPIC)
    if (WITH_FPIC)
      add_definitions(-fPIC)
    endif (WITH_FPIC)

    # with large file support
    execute_process(
      COMMAND
        getconf LFS64_CFLAGS
      OUTPUT_VARIABLE
        _lfs_CFLAGS
      ERROR_QUIET
      OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  else (CMAKE_SIZEOF_VOID_P MATCHES "8")
    # with large file support
    execute_process(
      COMMAND
        getconf LFS_CFLAGS
      OUTPUT_VARIABLE
        _lfs_CFLAGS
      ERROR_QUIET
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )
  endif (CMAKE_SIZEOF_VOID_P MATCHES "8")

  string(REGEX REPLACE "[\r\n]" " " ${_lfs_CFLAGS} "${${_lfs_CFLAGS}}")

  add_definitions(${_lfs_CFLAGS})

  add_definitions(-Wall -W -Wmissing-prototypes -Wdeclaration-after-statement -D_FORTIFY_SOURCE=2)
endif (UNIX AND NOT WIN32)
