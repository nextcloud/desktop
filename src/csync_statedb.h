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
 *
 * vim: ft=c.doxygen ts=2 sw=2 et cindent
 */

/**
 * @file csync_private.h
 *
 * @brief Private interface of csync
 *
 * @defgroup csyncJournalInternals csync journal internals
 * @ingroup csyncInternalAPI
 *
 * @{
 */

#ifndef _CSYNC_JOURNAL_H
#define _CSYNC_JOURNAL_H

#include "c_lib.h"
#include "csync_private.h"

void csync_set_journal_exists(CSYNC *ctx, int val);

int csync_get_journal_exists(CSYNC *ctx);

/**
 * @brief Load the journal.
 *
 * This function tries to load the journal. If it doesn't exists it creates
 * the sqlite3 database, but doesn't create the tables. This will be done when
 * csync gets destroyed.
 *
 * @param ctx      The csync context.
 * @param journal  Path to the journal file (sqlite3 db).
 *
 * @return 0 on success, less than 0 if an error occured with errno set.
 */
int csync_journal_load(CSYNC *ctx, const char *journal);

int csync_journal_write(CSYNC *ctx);

int csync_journal_close(CSYNC *ctx, const char *journal, int jwritten);

csync_file_stat_t *csync_journal_get_stat_by_hash(CSYNC *ctx, uint64_t phash);

csync_file_stat_t *csync_journal_get_stat_by_inode(CSYNC *ctx, ino_t inode);

/**
 * @brief A generic Journal query.
 *
 * @param ctx        The csync context.
 * @param statement  The SQL statement to execute
 * 
 * @return   A stringlist of the entries of a column. An emtpy stringlist if
 *           nothing has been found. NULL on error.
 */
c_strlist_t *csync_journal_query(CSYNC *ctx, const char *statement);

/**
 * @brief Insert function for the journal.
 *
 * @param ctx        The csync context.
 * @param statement  The SQL statement to insert into the journal.
 *
 * @return  The rowid of the most recent INSERT on success, 0 if the query
 *          wasn't successful.
 */
int csync_journal_insert(CSYNC *ctx, const char *statement);

int csync_journal_create_tables(CSYNC *ctx);

int csync_journal_drop_tables(CSYNC *ctx);

int csync_journal_insert_metadata(CSYNC *ctx);

/**
 * }@
 */
#endif /* _CSYNC_JOURNAL_H */

