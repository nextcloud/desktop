#ifndef OWNCLOUD_ASSERTS_H
#define OWNCLOUD_ASSERTS_H

#include <qglobal.h>

#if defined(QT_FORCE_ASSERTS) || !defined(QT_NO_DEBUG)
#define OC_ASSERT_MSG qFatal
#else
#define OC_ASSERT_MSG qCritical
#endif

// Default assert: If the condition is false in debug builds, terminate.
//
// Prints a message on failure, even in release builds.
#define OC_ASSERT(cond)                                                                 \
    if (!(cond)) {                                                                      \
        OC_ASSERT_MSG("ASSERT: \"%s\" in file %s, line %d", #cond, __FILE__, __LINE__); \
    } else {                                                                            \
    }
#define OC_ASSERT_X(cond, message)                                                                                \
    if (!(cond)) {                                                                                                \
        OC_ASSERT_MSG("ASSERT: \"%s\" in file %s, line %d with message: %s", #cond, __FILE__, __LINE__, message); \
    } else {                                                                                                      \
    }

// Enforce condition to be true, even in release builds.
//
// Prints 'message' and aborts execution if 'cond' is false.
#define OC_ENFORCE(cond)                                                          \
    if (!(cond)) {                                                                \
        qFatal("ENFORCE: \"%s\" in file %s, line %d", #cond, __FILE__, __LINE__); \
    } else {                                                                      \
    }
#define OC_ENFORCE_X(cond, message)                                                                         \
    if (!(cond)) {                                                                                          \
        qFatal("ENFORCE: \"%s\" in file %s, line %d with message: %s", #cond, __FILE__, __LINE__, message); \
    } else {                                                                                                \
    }

// An assert that is only present in debug builds: typically used for
// asserts that are too expensive for release mode.
//
// Q_ASSERT

#endif
