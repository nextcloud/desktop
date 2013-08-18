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

#include <stdlib.h>
#include <string.h>

#include "c_macro.h"
#include "c_alloc.h"
#include "c_dir.h"
#include "c_file.h"
#include "c_list.h"
#include "c_path.h"
#include "c_rbtree.h"
#include "c_string.h"
#include "c_time.h"
#include "c_private.h"

#ifndef UNIT_TESTING
#ifdef malloc
#undef malloc
#endif
#define malloc(x) DO_NOT_CALL_MALLOC__USE_C_MALLOC_INSTEAD

#ifdef calloc
#undef calloc
#endif
#define calloc(x,y) DO_NOT_CALL_CALLOC__USE_C_CALLOC_INSTEAD

#endif

#ifdef realloc
#undef realloc
#endif
#define realloc(x,y) DO_NOT_CALL_REALLOC__USE_C_REALLOC_INSTEAD

#ifdef dirname
#undef dirname
#endif
#define dirname(x) DO_NOT_CALL_MALLOC__USE_C_DIRNAME_INSTEAD

#ifdef basename
#undef basename
#endif
#define basename(x) DO_NOT_CALL_MALLOC__USE_C_BASENAME_INSTEAD

#ifdef strdup
#undef strdup
#endif
#define strdup(x) DO_NOT_CALL_STRDUP__USE_C_STRDUP_INSTEAD

