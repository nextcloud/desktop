#cmakedefine PACKAGE "${APPLICATION_NAME}"
#cmakedefine VERSION "${APPLICATION_VERSION}"
#cmakedefine LOCALEDIR "${LOCALE_INSTALL_DIR}"
#cmakedefine DATADIR "${DATADIR}"
#cmakedefine LIBDIR "${LIBDIR}"
#cmakedefine PLUGINDIR "${PLUGINDIR}"
#cmakedefine SYSCONFDIR "${SYSCONFDIR}"
#cmakedefine BINARYDIR "${BINARYDIR}"
#cmakedefine SOURCEDIR "${SOURCEDIR}"

#cmakedefine HAVE_CLOCK_GETTIME

#cmakedefine WITH_LOG4C 1
#cmakedefine WITH_ICONV 1

#cmakedefine HAVE_ARGP_H 1
#cmakedefine HAVE_ICONV_H 1
#cmakedefine HAVE_SYS_ICONV_H 1

#cmakedefine HAVE_TIMEGM 1
#cmakedefine HAVE_STRERROR_R 1
#cmakedefine HAVE_UTIMES 1
#cmakedefine HAVE_LSTAT 1
#cmakedefine HAVE_FNMATCH 1
#cmakedefine HAVE_ICONV 1
#cmakedefine HAVE_ICONV_CONST 1

#ifndef NEON_WITH_LFS
#cmakedefine NEON_WITH_LFS 1
#endif

#cmakedefine HAVE___MINGW_ASPRINTF 1
#cmakedefine HAVE_ASPRINTF 1

#cmakedefine WITH_UNIT_TESTING 1
