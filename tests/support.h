#ifndef _SUPPORT_H
#define _SUPPORT_H

#include <stdio.h>
#include <stdlib.h>
#include <check.h>

#include "csync_private.h"

/* create_case() with timeout of 30seconds (default) */
void create_case(Suite *s, const char *name, TFun function);

/* create_case() with timeout of 30seconds (default) and fixture */
void create_case_fixture(Suite *s, const char *name, TFun function, void (*setup)(void), void (*teardown)(void));

/*
 * create_case_timeout() allow to specific a specific timeout - intended for
 * breaking testcases which needs longer then 30seconds (default)
 */
void create_case_timeout(Suite *s, const char *name, TFun function,
    int timeout);

#endif /* _SUPPORT_H */
