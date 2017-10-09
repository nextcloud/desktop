/*
 * cynapses libc functions
 *
 * Copyright (c) 2008-2013 by Andreas Schneider <asn@cryptomilk.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * @file c_macro.h
 *
 * @brief cynapses libc macro definitions
 *
 * @defgroup cynMacroInternals cynapses libc macro definitions
 * @ingroup cynLibraryAPI
 *
 * @{
 */
#ifndef _C_MACRO_H
#define _C_MACRO_H

#include <stdint.h>
#include <string.h>

#define INT_TO_POINTER(i) (void *) i
#define POINTER_TO_INT(p) *((int *) (p))

/** Zero a structure */
#define ZERO_STRUCT(x) memset((char *)&(x), 0, sizeof(x))

/** Zero a structure given a pointer to the structure */
#define ZERO_STRUCTP(x) do { if ((x) != NULL) memset((char *)(x), 0, sizeof(*(x))); } while(0)

/** Free memory and zero the pointer */
#define SAFE_FREE(x) do { if ((x) != NULL) {free((void*)x); x=NULL;} } while(0)

/** Get the smaller value */
#define MIN(a,b) ((a) < (b) ? (a) : (b))

/** Get the bigger value */
#define MAX(a,b) ((a) < (b) ? (b) : (a))

/** Get the size of an array */
#define ARRAY_SIZE(a) (sizeof(a)/sizeof(a[0]))

/**
 * This is a hack to fix warnings. The idea is to use this everywhere that we
 * get the "discarding const" warning by the compiler. That doesn't actually
 * fix the real issue, but marks the place and you can search the code for
 * discard_const.
 *
 * Please use this macro only when there is no other way to fix the warning.
 * We should use this function in only in a very few places.
 *
 * Also, please call this via the discard_const_p() macro interface, as that
 * makes the return type safe.
 */
#define discard_const(ptr) ((void *)((uintptr_t)(ptr)))

/**
 * Type-safe version of discard_const
 */
#define discard_const_p(type, ptr) ((type *)discard_const(ptr))

#if (__GNUC__ >= 3)
# ifndef likely
#  define likely(x)   __builtin_expect(!!(x), 1)
# endif
# ifndef unlikely
#  define unlikely(x) __builtin_expect(!!(x), 0)
# endif
#else /* __GNUC__ */
# ifndef likely
#  define likely(x) (x)
# endif
# ifndef unlikely
#  define unlikely(x) (x)
# endif
#endif /* __GNUC__ */

/**
 * }@
 */

#ifdef _WIN32
/* missing errno codes on mingw */
#ifndef ENOTBLK
#define        ENOTBLK                15
#endif
#ifndef ETXTBSY
#define        ETXTBSY                26
#endif
#ifndef ENOBUFS
#define        ENOBUFS                WSAENOBUFS
#endif
#endif /* _WIN32 */

#endif /* _C_MACRO_H */

