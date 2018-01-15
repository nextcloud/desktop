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

#ifndef _CSYNC_UTIL_H
#define _CSYNC_UTIL_H

#include <stdint.h>

#include "csync_private.h"

const char OCSYNC_EXPORT *csync_instruction_str(enum csync_instructions_e instr);

void OCSYNC_EXPORT csync_memstat_check(void);

/* Returns true if we're reasonably certain that hash equality
 * for the header means content equality.
 *
 * Cryptographic safety is not required - this is mainly
 * intended to rule out checksums like Adler32 that are not intended for
 * hashing and have high likelihood of collision with particular inputs.
 */
bool OCSYNC_EXPORT csync_is_collision_safe_hash(const QByteArray &checksum_header);

#endif /* _CSYNC_UTIL_H */
