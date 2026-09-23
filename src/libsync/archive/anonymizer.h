/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef OCC_ARCHIVE_ANONYMIZER_H
#define OCC_ARCHIVE_ANONYMIZER_H

#include "owncloudlib.h"

#include <QHash>
#include <QList>
#include <QMap>
#include <QPair>
#include <QString>

namespace OCC
{

/**
 * @brief Builds and applies a consistent real to pseudonym mapping for debug archives.
 *
 * The same input always outputs the same pseudonym within one instance, so tokens
 * line up across logs, config and the sync database. Pure string transforms plus
 * map lookup, no I/O. The caller seeds known sensitive values before scanning.
 * @ingroup libsync
 */
class OWNCLOUDSYNC_EXPORT Anonymizer
{
public:
    Anonymizer() = default;

    /** Register a known username or display name. */
    void seedUser(const QString &user);

    /** Register a known server URL or host. */
    void seedServer(const QString &url);

    /** Register a known local or remote path (e.g. a sync root). */
    void seedPath(const QString &path);

    /**
     * Replace every seeded literal, plus any IP address, in a chunk of text.
     * Intended to be called per log line so large logs stream in bounded memory.
     */
    [[nodiscard]] QString anonymizeText(const QString &text);

    /** Map a filesystem path per segment, keeping the final extension. */
    [[nodiscard]] QString anonymizePath(const QString &path);

    /** Full real to pseudonym table for the sidecar, sorted by real value. */
    [[nodiscard]] QMap<QString, QString> mapping() const;

private:
    enum class Category {
        User,
        Server,
        Ip,
        Directory,
        File,
    };

    [[nodiscard]] QString tokenFor(const QString &real, Category category);
    [[nodiscard]] QString anonymizeIps(const QString &text);
    void addLiteral(const QString &real, const QString &token);

    QHash<QString, QString> _values; // real value -> pseudonym, one entry per distinct string
    QList<QPair<QString, QString>> _literals; // seeded real -> token, longest real first
    int _userCounter = 0;
    int _serverCounter = 0;
    int _ipCounter = 0;
    int _dirCounter = 0;
    int _fileCounter = 0;
};

} // namespace OCC

#endif // OCC_ARCHIVE_ANONYMIZER_H
