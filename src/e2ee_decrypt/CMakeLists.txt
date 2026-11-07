# SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

project(e2ee_decrypt)
set(CMAKE_AUTOMOC TRUE)

if(UNIX AND NOT APPLE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIE")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIE")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pie")
endif()

add_executable(nextcloud-e2ee-decrypt
    main.cpp
    e2eedecryptor.cpp
)

ecm_mark_nongui_executable(nextcloud-e2ee-decrypt)

target_compile_features(nextcloud-e2ee-decrypt PRIVATE cxx_std_20)
target_compile_definitions(nextcloud-e2ee-decrypt PRIVATE OPENSSL_SUPPRESS_DEPRECATED)

target_link_libraries(nextcloud-e2ee-decrypt
    PRIVATE
    Nextcloud::sync
    Qt::Core
    Qt::Network
    OpenSSL::Crypto
)

set_target_properties(nextcloud-e2ee-decrypt PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIRECTORY}
)

install(TARGETS nextcloud-e2ee-decrypt
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
