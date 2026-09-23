/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef OCC_ARCHIVE_DEBUGARCHIVEANONYMIZER_H
#define OCC_ARCHIVE_DEBUGARCHIVEANONYMIZER_H

#include "owncloudlib.h"

#include <QString>

namespace OCC
{

class Anonymizer;

/**
 * @brief File level anonymization steps for the debug archive.
 *
 * Each function reads a source file and writes an anonymized copy to a
 * destination, returning false on I/O failure. Seeding lives in the caller.
 * @ingroup libsync
 */
namespace DebugArchiveAnonymizer
{

/**
 * Anonymize a log file into a plain text copy. Handles both plain logs and
 * gzip compressed rotated logs (.gz), streaming line by line for bounded memory.
 * Output is always plain text; the archive container recompresses it.
 */
[[nodiscard]] OWNCLOUDSYNC_EXPORT bool anonymizeLogFile(Anonymizer &anonymizer, const QString &sourcePath, const QString &destinationPath);

/** Rewrite an INI config: pseudonymize values via the anonymizer and drop credential keys. */
[[nodiscard]] OWNCLOUDSYNC_EXPORT bool anonymizeConfigFile(Anonymizer &anonymizer, const QString &sourcePath, const QString &destinationPath);

/** Write the full real to pseudonym lookup table to a local file (kept out of the archive). */
[[nodiscard]] OWNCLOUDSYNC_EXPORT bool writeLookupTable(const Anonymizer &anonymizer, const QString &destinationPath);

} // namespace DebugArchiveAnonymizer

} // namespace OCC

#endif // OCC_ARCHIVE_DEBUGARCHIVEANONYMIZER_H
