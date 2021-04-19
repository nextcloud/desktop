# Keep in mind that some combinations will not work together
option(ENABLE_SANITIZER_ADDRESS "Enable address sanitizer to detect memory violations, buffer overflows, memory leaks" OFF)
option(ENABLE_SANITIZER_LEAK "Enable leak sanitizer to detect memory leaks" OFF)
option(ENABLE_SANITIZER_MEMORY "Enable memory sanitizer to detect reads in unitialized memory" OFF)
option(ENABLE_SANITIZER_UNDEFINED "Enable undefined sanitizer to detect undefined behavior" OFF)
option(ENABLE_SANITIZER_THREAD "Enable thread sanitizer to detect data races" OFF)

set(ENABLED_SANITIZERS)
mark_as_advanced(ENABLED_SANITIZERS)
macro(add_sanitizer_option variable flag)
  if(${variable})
    list(APPEND ENABLED_SANITIZERS ${flag})
  endif()
endmacro()

add_sanitizer_option(ENABLE_SANITIZER_ADDRESS "address")
add_sanitizer_option(ENABLE_SANITIZER_LEAK "leak")
add_sanitizer_option(ENABLE_SANITIZER_MEMORY "memory")
add_sanitizer_option(ENABLE_SANITIZER_UNDEFINED "undefined")
add_sanitizer_option(ENABLE_SANITIZER_THREAD "thread")

function(enable_sanitizers target)
  if(ENABLED_SANITIZERS)
    string(REPLACE ";" "," ENABLED_SANITIZER_FLAGS "${ENABLED_SANITIZERS}")
    message(STATUS "Enabled ${ENABLED_SANITIZER_FLAGS} sanitizers on ${target}")


    target_compile_options(${target} PRIVATE
      $<$<CXX_COMPILER_ID:MSVC>:-fsanitize=${ENABLED_SANITIZER_FLAGS}>
      $<$<CXX_COMPILER_ID:Clang>:-fsanitize=${ENABLED_SANITIZER_FLAGS} -g>
      $<$<CXX_COMPILER_ID:GNU>:-fsanitize=${ENABLED_SANITIZER_FLAGS} -g>
      )

    # Until version 16.9 Preview 2 of Visual Studio, we need to link manually against the asan libs
    # https://devblogs.microsoft.com/cppblog/addresssanitizer-asan-for-windows-with-msvc/#compiling-with-asan-from-the-console
    # Please make also sure that Windows can find the dll at runtime. You may need to add
    # C:/ProgramFiles (x86)/Microsoft Visual Studio/2019/Community/VC/Tools/Llvm/x64/lib/clang/10.0.0/lib/windows
    # to your path.
    target_link_directories(${target} PRIVATE
      $<$<CXX_COMPILER_ID:MSVC>:$ENV{VSINSTALLDIR}/VC/Tools/Llvm/x64/lib/clang/10.0.0/lib/windows>
    )
    target_link_libraries(${target} PRIVATE
      "$<$<CXX_COMPILER_ID:MSVC>:clang_rt.asan_dynamic-x86_64;clang_rt.asan_dynamic_runtime_thunk-x86_64>"
    )
    target_link_options(${target} PRIVATE
      $<$<CXX_COMPILER_ID:MSVC>:/wholearchive:clang_rt.asan_dynamic_runtime_thunk-x86_64.lib /wholearchive:clang_rt.asan_dynamic-x86_64.lib>
    )
  endif()
endfunction()
