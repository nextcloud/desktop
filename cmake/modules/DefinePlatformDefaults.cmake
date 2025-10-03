# (c) 2014 Copyright ownCloud GmbH
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING* file.

# Set system vars

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    set(LINUX TRUE)
endif(CMAKE_SYSTEM_NAME MATCHES "Linux")

if (CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
    set(FREEBSD TRUE)
    set(BSD TRUE)
endif (CMAKE_SYSTEM_NAME MATCHES "FreeBSD")

if (CMAKE_SYSTEM_NAME MATCHES "OpenBSD")
    set(OPENBSD TRUE)
    set(BSD TRUE)
endif (CMAKE_SYSTEM_NAME MATCHES "OpenBSD")

if (CMAKE_SYSTEM_NAME MATCHES "NetBSD")
    set(NETBSD TRUE)
    set(BSD TRUE)
endif (CMAKE_SYSTEM_NAME MATCHES "NetBSD")

if (CMAKE_SYSTEM_NAME MATCHES "(Solaris|SunOS)")
    set(SOLARIS TRUE)
endif (CMAKE_SYSTEM_NAME MATCHES "(Solaris|SunOS)")

if (CMAKE_SYSTEM_NAME MATCHES "OS2")
    set(OS2 TRUE)
endif (CMAKE_SYSTEM_NAME MATCHES "OS2")
