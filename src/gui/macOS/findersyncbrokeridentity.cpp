/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "findersyncbrokeridentity.h"

#include "config.h"

#include <QStringList>

namespace OCC::Mac::FinderSyncBrokerIdentity {

QString appGroupIdentifier()
{
    return QStringLiteral(DEVELOPMENT_TEAM) + QLatin1Char('.') + QStringLiteral(APPLICATION_REV_DOMAIN);
}

QString brokerServiceName()
{
    // Must stay in lockstep with three other places, all of which are asserted against this
    // function by testfindersyncbrokeridentity:
    //   - PRODUCT_BUNDLE_IDENTIFIER of the FinderSyncBroker Xcode target
    //   - the install destination in shell_integration/MacOSX/CMakeLists.txt, because a login
    //     item's wrapper filename must equal its bundle identifier
    //   - the extension's own derivation from NCApplicationGroupIdentifier
    return appGroupIdentifier() + QStringLiteral(".FinderSyncBroker");
}

QString peerRequirement()
{
    const QStringList identifiers = {
        QStringLiteral(APPLICATION_REV_DOMAIN),                          // the desktop client
        QStringLiteral(APPLICATION_REV_DOMAIN) + QStringLiteral(".FinderSyncExt"), // the extension
        brokerServiceName(),                                            // the broker login item
    };

    QStringList identifierClauses;
    identifierClauses.reserve(identifiers.size());
    for (const auto &identifier : identifiers) {
        identifierClauses.append(QStringLiteral("identifier \"%1\"").arg(identifier));
    }

    QStringList certificateClauses = {
        // Mac App Store distribution.
        QStringLiteral("certificate leaf[field.1.2.840.113635.100.6.1.9]"),
        // Developer ID distribution, pinned to whichever team built this bundle — DEVELOPMENT_TEAM
        // is a CMake cache variable, so a branded build signed by another team pins that team.
        // It must match the team the binaries are actually signed with, which is already required
        // for the App Group entitlement below to be honoured at all.
        QStringLiteral("(certificate 1[field.1.2.840.113635.100.6.2.6] "
                       "and certificate leaf[field.1.2.840.113635.100.6.1.13] "
                       "and certificate leaf[subject.OU] = \"%1\")")
            .arg(QStringLiteral(DEVELOPMENT_TEAM)),
    };

#ifndef QT_NO_DEBUG
    // Locally built peers are signed with an Apple Development certificate, whose designated
    // requirement is not compatible with either clause above. Without this a debug build
    // cannot talk to its own broker at all.
    certificateClauses.append(QStringLiteral("(certificate 1[field.1.2.840.113635.100.6.2.1] "
                                             "and certificate leaf[field.1.2.840.113635.100.6.1.12] "
                                             "and certificate leaf[subject.OU] = \"%1\")")
                                  .arg(QStringLiteral(DEVELOPMENT_TEAM)));
#endif

    return QStringLiteral("anchor apple generic and (%1) and (%2)")
        .arg(identifierClauses.join(QStringLiteral(" or ")), certificateClauses.join(QStringLiteral(" or ")));
}

} // namespace OCC::Mac::FinderSyncBrokerIdentity
