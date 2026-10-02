/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "fileproviderdomainmapping.h"

#include <algorithm>

namespace OCC::Mac::FileProviderDomainMapping
{

QString domainIdentifierToAdopt(const QString &accountIdentifier,
                                const QString &configuredVolumeUuid,
                                const QString &configuredDomainIdentifier,
                                const QList<DomainDescriptor> &domains)
{
    if (accountIdentifier.isEmpty() || configuredVolumeUuid.isEmpty()) {
        return {};
    }

    if (!configuredDomainIdentifier.isEmpty()) {
        const auto configuredStillRegistered = std::any_of(domains.cbegin(), domains.cend(), [&](const auto &domain) {
            return domain.identifier == configuredDomainIdentifier;
        });
        if (configuredStillRegistered) {
            return {};
        }
    }

    QStringList candidates;
    for (const auto &domain : domains) {
        if (!domain.isExternal() || domain.accountIdentifier != accountIdentifier
            || domain.volumeUuid.compare(configuredVolumeUuid, Qt::CaseInsensitive) != 0) {
            continue;
        }
        candidates.append(domain.identifier);
    }

    std::sort(candidates.begin(), candidates.end());
    return candidates.isEmpty() ? QString{} : candidates.constFirst();
}

QString domainIdentifierToReconnect(const QString &configuredDomainIdentifier, const bool accountConnected, const QList<DomainDescriptor> &domains)
{
    if (!accountConnected || configuredDomainIdentifier.isEmpty()) {
        return {};
    }

    const auto domain = std::find_if(domains.cbegin(), domains.cend(), [&](const auto &candidate) {
        return candidate.identifier == configuredDomainIdentifier && candidate.disconnected;
    });
    return domain == domains.cend() ? QString{} : domain->identifier;
}

QStringList removableOrphanDomainIdentifiers(const QSet<QString> &configuredDomainIdentifiers, const QList<DomainDescriptor> &domains)
{
    QStringList removable;
    for (const auto &domain : domains) {
        if (domain.isExternal() || configuredDomainIdentifiers.contains(domain.identifier)) {
            continue;
        }
        removable.append(domain.identifier);
    }
    return removable;
}

} // namespace OCC::Mac::FileProviderDomainMapping
