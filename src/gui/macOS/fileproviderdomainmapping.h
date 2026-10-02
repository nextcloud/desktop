/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

namespace OCC::Mac::FileProviderDomainMapping
{

struct DomainDescriptor {
    QString identifier;
    QString accountIdentifier;
    QString volumeUuid;
    bool disconnected = false;

    [[nodiscard]] bool isExternal() const
    {
        return !volumeUuid.isEmpty();
    }
};

/**
 * Select a registered external-volume domain for an account whose persisted domain
 * identifier is missing or stale. A still-registered persisted identifier always wins.
 * Matching candidates are ordered by identifier so repeated reconciliation is stable.
 */
[[nodiscard]] QString domainIdentifierToAdopt(const QString &accountIdentifier,
                                              const QString &configuredVolumeUuid,
                                              const QString &configuredDomainIdentifier,
                                              const QList<DomainDescriptor> &domains);

/**
 * Return a locally-owned domain that should be reconnected after becoming visible again.
 * Only disconnected domains belonging to a connected account are eligible.
 */
[[nodiscard]] QString domainIdentifierToReconnect(const QString &configuredDomainIdentifier, bool accountConnected, const QList<DomainDescriptor> &domains);

/**
 * Return domains that are safe for the client to remove as orphans. External-volume
 * domains without a local mapping are intentionally retained because they may have been
 * created on another Mac and travelled with the volume.
 */
[[nodiscard]] QStringList removableOrphanDomainIdentifiers(const QSet<QString> &configuredDomainIdentifiers, const QList<DomainDescriptor> &domains);

} // namespace OCC::Mac::FileProviderDomainMapping
