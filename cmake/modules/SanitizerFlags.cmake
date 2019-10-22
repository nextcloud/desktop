
# Enable address sanitizer (gcc/clang only)
macro(ENABLE_SANITIZER)

  if (NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    message(FATAL_ERROR "Sanitizer supported only for gcc/clang")
  endif()

  set(SANITIZER_FLAGS "-fsanitize=address  -fsanitize=leak -g")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${SANITIZER_FLAGS}")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${SANITIZER_FLAGS}")

  set(LINKER_FLAGS "-fsanitize=address,undefined -fuse-ld=gold")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${LINKER_FLAGS}")

endmacro()

