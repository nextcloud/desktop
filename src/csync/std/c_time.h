/*
 * c_time - time functions
 *
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-FileCopyrightText: 2008-2013 by Andreas Schneider <asn@cryptomilk.org>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef _C_TIME_H
#define _C_TIME_H

#include <QString>

#include "ocsynclib.h"

#ifdef _WIN32
#include <time.h>
#else
#include <sys/time.h>
#endif

OCSYNC_EXPORT int c_utimes(const QString &uri, time_t time);


#endif /* _C_TIME_H */
