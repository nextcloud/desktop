/*
 * libcsync -- a library to sync a directory with another
 *
 * Copyright (c) 2012      by Klaas Freitag <freitag@owncloud.com>
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
 * @file csync_dbtree.h
 *
 * @brief Private interface of csync
 *
 * @defgroup csyncdbtreeInternals csync statedb internals
 * @ingroup csyncInternalAPI
 *
 * @{
 */

#ifndef _CSYNC_DBTREE_H
#define _CSYNC_DBTREE_H

#include "c_lib.h"
#include "csync_private.h"
#include "vio/csync_vio_handle.h"

/**
 * @brief Open a directory based on the statedb.
 *
 * This function reads the list of files within a directory from statedb and
 * builds up a list in memory.
 *
 * @param ctx      The csync context.
 * @param name     The directory name.
 *
 * @return 0 on success, less than 0 if an error occured with errno set.
 */
csync_vio_method_handle_t *csync_dbtree_opendir(CSYNC *ctx, const char *name);

int csync_dbtree_closedir(CSYNC *ctx, csync_vio_method_handle_t *dhandle);

csync_vio_file_stat_t *csync_dbtree_readdir(CSYNC *ctx, csync_vio_method_handle_t *dhandle);

/**
 * }@
 */
#endif /* _CSYNC_DBTREE_H */
/* vim: set ft=c.doxygen ts=8 sw=2 et cindent: */
