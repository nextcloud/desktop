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

#ifdef malloc
#undef malloc
#endif
#define malloc(x) DO_NOT_CALL_MALLOC__USE_C_MALLOC_INSTEAD

#ifdef calloc
#undef calloc
#endif
#define calloc(x,y) DO_NOT_CALL_CALLOC__USE_C_CALLOC_INSTEAD

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

