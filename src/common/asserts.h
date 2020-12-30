#ifndef OWNCLOUD_ASSERTS_H
#define OWNCLOUD_ASSERTS_H

#include <qglobal.h>

#if defined(QT_FORCE_ASSERTS) || !defined(QT_NO_DEBUG)
#define OC_ASSERT_MSG qFatal
#else
#define OC_ASSERT_MSG qCritical
#endif

// For overloading macros by argument count
// See stackoverflow.com/questions/16683146/can-macros-be-overloaded-by-number-of-arguments
// Bugfix 08/09/2019: Broken arg expansion led to always collapsing to 1 arg (XXXX_1 overload result)
// See also: https://stackoverflow.com/questions/9183993/msvc-variadic-macro-expansion
#define OC_ASSERT_GLUE(x, y) x y

#define OC_ASSERT_GET_COUNT(_1, _2, _3, COUNT, ...) COUNT
#define OC_ASSERT_EXPAND_ARGS(args) OC_ASSERT_GET_COUNT args
#define OC_ASSERT_VA_SIZE(...) OC_ASSERT_EXPAND_ARGS((__VA_ARGS__, 3, 2, 1, 0))

#define OC_ASSERT_SELECT2(NAME, COUNT) NAME##COUNT
#define OC_ASSERT_SELECT1(NAME, COUNT) OC_ASSERT_SELECT2(NAME, COUNT)
#define OC_ASSERT_SELECT(NAME, COUNT) OC_ASSERT_SELECT1(NAME, COUNT)

#define OC_ASSERT_OVERLOAD(NAME, ...) OC_ASSERT_GLUE(OC_ASSERT_SELECT(NAME, OC_ASSERT_VA_SIZE(__VA_ARGS__)), \
    (__VA_ARGS__))

// Default assert: If the condition is false in debug builds, terminate.
//
// Prints a message on failure, even in release builds.
#define ASSERT1(cond)                                                                  \
    if (!(cond)) {                                                                      \
        OC_ASSERT_MSG("ASSERT: \"%s\" in file %s, line %d", #cond, __FILE__, __LINE__); \
    } else {                                                                            \
    }
#define ASSERT2(cond, message)                                                                                   \
    if (!(cond)) {                                                                                                \
        OC_ASSERT_MSG("ASSERT: \"%s\" in file %s, line %d with message: %s", #cond, __FILE__, __LINE__, message); \
    } else {                                                                                                      \
    }
#define ASSERT(...) OC_ASSERT_OVERLOAD(ASSERT, __VA_ARGS__)

// Enforce condition to be true, even in release builds.
//
// Prints 'message' and aborts execution if 'cond' is false.
#define ENFORCE1(cond)                                                           \
    if (!(cond)) {                                                                \
        qFatal("ENFORCE: \"%s\" in file %s, line %d", #cond, __FILE__, __LINE__); \
    } else {                                                                      \
    }
#define ENFORCE2(cond, message)                                                                            \
    if (!(cond)) {                                                                                          \
        qFatal("ENFORCE: \"%s\" in file %s, line %d with message: %s", #cond, __FILE__, __LINE__, message); \
    } else {                                                                                                \
    }
#define ENFORCE(...) OC_ASSERT_OVERLOAD(ENFORCE, __VA_ARGS__)

// An assert that is only present in debug builds: typically used for
// asserts that are too expensive for release mode.
//
// Q_ASSERT

#endif
