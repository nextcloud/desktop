/*
 * libcsync -- a library to sync a directory with another
 *
 * Copyright (c) 2008-2013 by Andreas Schneider <asn@cryptomilk.org>
 * Copyright (c) 2012-2013 by Klaas Freitag <freitag@owncloud.com>
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
int csync_statedb_load(CSYNC *ctx, const char *statedb, sqlite3 **pdb);

int csync_statedb_close(const char *statedb, sqlite3 *db, int jwritten);

csync_file_stat_t *csync_statedb_get_stat_by_hash(sqlite3 *db, uint64_t phash);

csync_file_stat_t *csync_statedb_get_stat_by_inode(sqlite3 *db, uint64_t inode);

csync_file_stat_t *csync_statedb_get_stat_by_file_id( sqlite3 *db,
                                                     const char *file_id );

char *csync_statedb_get_uniqId(CSYNC *ctx, uint64_t jHash, csync_vio_file_stat_t *buf);

/**
 * @brief Query all files metadata inside and below a path.
 * @param ctx        The csync context.
 * @param path       The path.
 *
 * This function queries all metadata of all files inside or below the
 * given path. The result is a linear string list with a multiple of 9
 * entries. For each result file there are 9 strings which are phash,
 * path, inode, uid, gid, mode, modtime, type and md5 (unique id).
 *
 * Note that not only the files in the given path are part of the result
 * but also the files in directories below the given path. Ie. if the
 * parameter path is /home/kf/test, we have /home/kf/test/file.txt in
 * the result but also /home/kf/test/homework/another_file.txt
 *
 * @return   A stringlist containing a multiple of 9 entries.
 */
c_strlist_t *csync_statedb_get_below_path(CSYNC *ctx, const char *path);

/**
 * @brief A generic statedb query.
 *
 * @param ctx        The csync context.
 * @param statement  The SQL statement to execute
 * 
 * @return   A stringlist of the entries of a column. An emtpy stringlist if
 *           nothing has been found. NULL on error.
 */
c_strlist_t *csync_statedb_query(sqlite3 *db, const char *statement);

/**
 * @brief Insert function for the statedb.
 *
 * @param ctx        The csync context.
 * @param statement  The SQL statement to insert into the statedb.
 *
 * @return  The rowid of the most recent INSERT on success, 0 if the query
 *          wasn't successful.
 */
typedef struct csync_progressinfo_s {
  struct csync_progressinfo_s *next;
  uint64_t phash;
  uint64_t modtime;
  char *md5;
  int error;
  int chunk;
  int transferId;
  char *tmpfile;
  char *error_string;
} csync_progressinfo_t;

/**
 * }@
 */
#endif /* _CSYNC_STATEDB_H */
/* vim: set ft=c.doxygen ts=8 sw=2 et cindent: */
