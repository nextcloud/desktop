/*
 * cynapses libc functions
 *
 * Copyright (c) 2008      by Andreas Schneider <mail@cynapses.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * vim: ts=2 sw=2 et cindent
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

