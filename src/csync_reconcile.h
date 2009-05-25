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

#ifndef _CSYNC_RECONCILE_H
#define _CSYNC_RECONCILE_H

/**
 * @file csync_reconcile.h
 *
 * @brief Reconciliation
 *
 * We merge replicas at the file level. The merged replica contains the
 * superset of files that are on the local machine and server copies of
 * the replica. In the case where the same file is in both the local
 * and server copy, the file that was modified most recently is used.
 * This means that new files are not deleted, and updated versions of
 * existing files are not overwritten.
 *
 * When a file is updated, the merge algorithm compares the destination
 * file with the the source file. If the destination file is newer
 * (timestamp is newer), it is not overwritten. If both files, on the
 * source and the destination, have been changed, the newer file wins.
 *
 * @defgroup csyncReconcilationInternals csync reconciliation internals
 * @ingroup csyncInternalAPI
 *
 * @{
 */

/**
 * @brief Reconcile the files.
 *
 * @param  ctx          The csync context to use.
 *
 * @return 0 on success, < 0 on error.
 *
 * @todo Add an argument to set the algorithm to use.
 */
int csync_reconcile_updates(CSYNC *ctx);

/**
 * }@
 */
#endif /* _CSYNC_RECONCILE_H */

/* vim: set ft=c.doxygen ts=8 sw=2 et cindent: */
