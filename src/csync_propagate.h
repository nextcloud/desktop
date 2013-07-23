/*
 * libcsync -- a library to sync a directory with another
 *
 * Copyright (c) 2008-2013 by Andreas Schneider <asn@cryptomilk.org>
 * Copyright (c) 2012-2013 by Klaas Freitag <freitag@owncloud.com>wie
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

#ifndef _CSYNC_PROPAGATE_H
#define _CSYNC_PROPAGATE_H

#include <sys/types.h>

/**
 * @file csync_reconcile.h
 *
 * @brief Propagation
 *
 * It uses the calculated records to apply them on the current replica. The
 * propagator uses a two-phase-commit mechanism to simulate an atomic
 * filesystem operation.
 *
 * In the first phase we copy the file to a temporary file on the opposite
 * replica. This has the advantage that we can check if file which has been
 * copied to the opposite replica has been transfered successfully. If the
 * connection gets interrupted during the transfer we still have the original
 * states of the file. This means no data will be lost.
 *
 * In the second phase the file on the opposite replica will be overwritten by
 * the temporary file.
 *
 * After a successful propagation we have to merge the trees to reflect the
 * current state of the filesystem tree. This updated tree will be written as a
 * journal into the state database. It will be used during the update detection
 * of the next synchronization. See above for a description of the state
 * database during synchronization.
 *
 * @defgroup csyncPropagationInternals csync propagation internals
 * @ingroup csyncInternalAPI
 *
 * @{
 */

#define C_FILE_MODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)
#define C_DIR_MODE (S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH)

/**
 * @brief Propagate all files.
 *
 * @param  ctx          The csync context to use for propagation.
 *
 * @return 0 on success, < 0 on error.
 */
int csync_propagate_files(CSYNC *ctx);

/**
 * @brief Initialize the data base for overall progress information.
 *
 * @param  ctx          The csync context to use for propagation.
 *
 * @return 0 on success, < 0 on error.
 */
int csync_init_overall_progress(CSYNC *ctx);

/**
 * }@
 */
#endif /* _CSYNC_PROPAGATE_H */

/* vim: set ft=c.doxygen ts=8 sw=2 et cindent: */
