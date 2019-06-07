# (c) 2019 Copyright ownCloud GmbH
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING* file.

find_path(LIBCLOUDPROVIDERS_INCLUDE_DIR
            NAMES
              cloudprovidersaccountexporter.h
              cloudprovidersproviderexporter.h
            PATH_SUFFIXES
              cloudproviders
            )

find_library(LIBCLOUDPROVIDERS_LIBRARY
            NAMES
              cloudproviders
            PATHS
              /usr/lib
              /usr/lib/${CMAKE_ARCH_TRIPLET}
              /usr/local/lib
              ${CMAKE_LIBRARY_PATH}
              ${CMAKE_INSTALL_PREFIX}/lib
            )

# Using version <0.3.0 would lead to crashes during runtime when accounts are unexported.
get_filename_component(LIBCLOUDPROVIDERS_LIBRARY_REALPATH ${LIBCLOUDPROVIDERS_LIBRARY} REALPATH)
if(${LIBCLOUDPROVIDERS_LIBRARY_REALPATH} MATCHES "\\.([0-9]*)\\.([0-9]*)\\.([0-9]*)$")
    if ((${CMAKE_MATCH_1} EQUAL 0) AND (${CMAKE_MATCH_2} LESS 3))
        message("libcloudproviders version is older than 0.3.0, not enabling it")
        set(LIBCLOUDPROVIDERS_LIBRARY "")
    endif()
else()
    message("Can't determine libcloudproviders version, not enabling it")
    set(LIBCLOUDPROVIDERS_LIBRARY "")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
        LIBCLOUDPROVIDERS
	DEFAULT_MSG
	LIBCLOUDPROVIDERS_LIBRARY LIBCLOUDPROVIDERS_INCLUDE_DIR)

mark_as_advanced(LIBCLOUDPROVIDERS_INCLUDE_DIR LIBCLOUDPROVIDERS_LIBRARY)
