if (EXISTS "${PROJECT_SOURCE_DIR}/branding")
    set(OEM_THEME_DIR "${PROJECT_SOURCE_DIR}/branding" CACHE STRING "The directory containing a custom theme")
else()
    set(OEM_THEME_DIR "${PROJECT_SOURCE_DIR}/src/resources/" CACHE STRING "Define directory containing a custom theme")
endif()

if (EXISTS "${OEM_THEME_DIR}/OEM.cmake")
    include("${OEM_THEME_DIR}/OEM.cmake")
else()
    include ("${CMAKE_CURRENT_LIST_DIR}/OWNCLOUD.cmake")
endif()

message(STATUS "Branding: ${APPLICATION_NAME}")

# Default suffix if the theme doesn't define one
if(NOT DEFINED APPLICATION_VIRTUALFILE_SUFFIX)
    set(APPLICATION_VIRTUALFILE_SUFFIX "${APPLICATION_SHORTNAME}_virtual" CACHE STRING "Virtual file suffix (not including the .)")
endif()

# Default dbus name and path
if(NOT DEFINED APPLICATION_CLOUDPROVIDERS_DBUS_NAME)
    set(APPLICATION_CLOUDPROVIDERS_DBUS_NAME ${APPLICATION_REV_DOMAIN})
endif()
if(NOT DEFINED APPLICATION_CLOUDPROVIDERS_DBUS_PATH)
    set(APPLICATION_CLOUDPROVIDERS_DBUS_PATH "/${APPLICATION_CLOUDPROVIDERS_DBUS_NAME}")
    string(REPLACE "." "/" APPLICATION_CLOUDPROVIDERS_DBUS_PATH ${APPLICATION_CLOUDPROVIDERS_DBUS_PATH})
endif()

# need this logic to not mess with re/uninstallations via macosx.pkgproj
if(${APPLICATION_REV_DOMAIN} STREQUAL "com.owncloud.desktopclient")
    set(APPLICATION_REV_DOMAIN_INSTALLER "com.ownCloud.client")
else()
    set(APPLICATION_REV_DOMAIN_INSTALLER ${APPLICATION_REV_DOMAIN})
endif()

# For usage in XML files we preprocess
string(REPLACE "&" "&amp;" APPLICATION_NAME_XML_ESCAPED "${APPLICATION_NAME}")
string(REPLACE "<" "&lt;" APPLICATION_NAME_XML_ESCAPED "${APPLICATION_NAME_XML_ESCAPED}")
string(REPLACE ">" "&gt;" APPLICATION_NAME_XML_ESCAPED "${APPLICATION_NAME_XML_ESCAPED}")

if (NOT DEFINED LINUX_PACKAGE_SHORTNAME)
    set(LINUX_PACKAGE_SHORTNAME "${APPLICATION_SHORTNAME}")
endif()

if (NOT DEFINED PACKAGE)
    set(PACKAGE "${LINUX_PACKAGE_SHORTNAME}-client")
endif()

set(PROJECT_NAME "${PACKAGE}")

if(NOT CRASHREPORTER_EXECUTABLE)
    set(CRASHREPORTER_EXECUTABLE "${APPLICATION_EXECUTABLE}_crash_reporter")
endif()


include("${CMAKE_CURRENT_LIST_DIR}/VERSION.cmake")
