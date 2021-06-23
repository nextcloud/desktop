/*
 * Copyright (C) by Hannah von Reth <hannah.vonreth@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */
#pragma once
#include <qglobal.h>

#ifdef Q_CC_MSVC
// _Pragma would require C99 ecm currently sets C90
#define OC_DISABLE_DEPRECATED_WARNING \
    __pragma(warning(push));          \
    __pragma(warning(disable : 4996))

#define OC_ENABLE_DEPRECATED_WARNING __pragma(warning(pop))
#else
#define OC_DISABLE_DEPRECATED_WARNING \
    _Pragma("GCC diagnostic push");   \
    _Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")

#define OC_ENABLE_DEPRECATED_WARNING _Pragma("GCC diagnostic pop")
#endif
