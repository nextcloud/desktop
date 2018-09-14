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

#ifndef _CSYNC_RECONCILE_H
#define _CSYNC_RECONCILE_H

/**
 * @file csync_reconcile.h
 *
 * @brief Reconciliation
 *
 * The most important component is the update detector, because the reconciler
 * depends on it. The correctness of reconciler is mandatory because it can
 * damage a filesystem. It decides which file:
 *
 *       - stays untouched
 *       - has a conflict
 *       - gets synchronized
 *       - or is deleted.
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
 * @todo Add an argument to set the algorithm to use.
 */
void OCSYNC_EXPORT csync_reconcile_updates(CSYNC *ctx);

/**
 * }@
 */
#endif /* _CSYNC_RECONCILE_H */

/* vim: set ft=c.doxygen ts=8 sw=2 et cindent: */
