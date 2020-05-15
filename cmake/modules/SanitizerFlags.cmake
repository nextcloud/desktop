# Enable address sanitizer (gcc/clang only)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(SANITIZERS)

    macro(add_sanitizer_option variable flag help)
        option(${variable} "Enable ${help}" OFF)
        if(${variable})
            list(APPEND SANITIZERS ${flag})
        endif()
        mark_as_advanced(${variable})
    endmacro()

    add_sanitizer_option(SANITIZE_ADDRESS "address"
        "AddressSanitizer (detects memory violations, buffer overflows, memory leaks)")
    add_sanitizer_option(SANITIZE_LEAK "leak"
        "standalone LeakSanitizer (detects memory leaks only)")
    add_sanitizer_option(SANITIZE_MEMORY "memory"
        "MemorySanitizer (detects reads in uninitialized memory)")
    add_sanitizer_option(SANITIZE_UNDEFINED "undefined"
        "UndefinedBehaviorSanitizer (detects undefined behavior)")
    add_sanitizer_option(SANITIZE_THREAD "thread"
        "ThreadSanitizer (detects data races)")

    if(SANITIZERS)
        string(REPLACE ";" "," SANITIZER_FLAGS "${SANITIZERS}")
        set(SANITIZER_FLAGS "-fsanitize=${SANITIZER_FLAGS}")
        string(APPEND CMAKE_CXX_FLAGS " ${SANITIZER_FLAGS}")
        string(APPEND CMAKE_C_FLAGS " ${SANITIZER_FLAGS}")
        string(APPEND CMAKE_EXE_LINKER_FLAGS " ${SANITIZER_FLAGS}")
    endif()
endif()
