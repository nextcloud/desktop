/*
 * libcsync -- a library to sync a directory with another
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

#ifndef _CSYNC_LOCK_H
#define _CSYNC_LOCK_H

#include "csync.h"

/**
 * @file csync_lock.h
 *
 * @brief File locking
 *
 * This prevents csync to start the same synchronization task twice which could
 * lead to several problems.
 *
 * @defgroup csyncLockingInternals csync file lockling internals
 * @ingroup csyncInternalAPI
 *
 * @{
 */

/**
 * @brief Lock the client if possible.
 *
 * This functiion tries to lock the client with a lock file.
 *
 * @param lockfile  The lock file to create.
 *
 * @return  0 if the lock was successfull, less than 0 if the lock file
 *          couldn't be created or if it is already locked.
 */
int csync_lock(const char *lockfile);

/**
 * @brief  Remove the lockfile
 *
 * Only our own lock can be removed. This function can't remove a lock from
 * another client.
 *
 * @param lockfile  The lock file to remove.
 */
void csync_lock_remove(const char *lockfile);

/**
 * }@
 */
#endif /* _CSYNC_LOCK_H */
/* vim: set ft=c.doxygen ts=8 sw=2 et cindent: */
