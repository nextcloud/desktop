/*
 * libcsync -- a library to sync a directory with another
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
 */

/**
 * @file csync_private.h
 *
 * @brief Private interface of csync
 *
 * @defgroup csyncstatedbInternals csync statedb internals
 * @ingroup csyncInternalAPI
 *
 * @{
 */

#ifndef _CSYNC_STATEDB_H
#define _CSYNC_STATEDB_H

#include "c_lib.h"
#include "csync_private.h"

void csync_set_statedb_exists(CSYNC *ctx, int val);

int csync_get_statedb_exists(CSYNC *ctx);

/**
 * @brief Load the statedb.
 *
 * This function tries to load the statedb. If it doesn't exists it creates
 * the sqlite3 database, but doesn't create the tables. This will be done when
 * csync gets destroyed.
 *
 * @param ctx      The csync context.
 * @param statedb  Path to the statedb file (sqlite3 db).
 *
 * @return 0 on success, less than 0 if an error occured with errno set.
 */
int csync_statedb_load(CSYNC *ctx, const char *statedb);

int csync_statedb_write(CSYNC *ctx);

int csync_statedb_close(CSYNC *ctx, const char *statedb, int jwritten);

csync_file_stat_t *csync_statedb_get_stat_by_hash(CSYNC *ctx, uint64_t phash);

csync_file_stat_t *csync_statedb_get_stat_by_inode(CSYNC *ctx, uint64_t inode);

/**
 * @brief A generic statedb query.
 *
 * @param ctx        The csync context.
 * @param statement  The SQL statement to execute
 * 
 * @return   A stringlist of the entries of a column. An emtpy stringlist if
 *           nothing has been found. NULL on error.
 */
c_strlist_t *csync_statedb_query(CSYNC *ctx, const char *statement);

/**
 * @brief Insert function for the statedb.
 *
 * @param ctx        The csync context.
 * @param statement  The SQL statement to insert into the statedb.
 *
 * @return  The rowid of the most recent INSERT on success, 0 if the query
 *          wasn't successful.
 */
int csync_statedb_insert(CSYNC *ctx, const char *statement);

int csync_statedb_create_tables(CSYNC *ctx);

int csync_statedb_drop_tables(CSYNC *ctx);

int csync_statedb_insert_metadata(CSYNC *ctx);

/**
 * }@
 */
#endif /* _CSYNC_STATEDB_H */
/* vim: set ft=c.doxygen ts=8 sw=2 et cindent: */
