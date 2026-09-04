/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "fileproviderdomainidentifierpolicy.h"

namespace OCC::Mac::FileProviderDomainIdentifierPolicy {

RegistrationDecision decideRegistration(const QString &storedIdentifier,
                                        const QSet<QString> &registeredIdentifiers,
                                        const bool domainListingSucceeded)
{
    auto decision = RegistrationDecision{};

    if (!domainListingSucceeded) {
        decision.action = RegistrationAction::Abort;
        return decision;
    }

    if (!storedIdentifier.isEmpty() && registeredIdentifiers.contains(storedIdentifier)) {
        decision.action = RegistrationAction::Skip;
        return decision;
    }

    if (!storedIdentifier.isEmpty()) {
        decision.action = RegistrationAction::AddStored;
        return decision;
    }

    decision.action = RegistrationAction::AddFresh;
    return decision;
}

} // namespace OCC::Mac::FileProviderDomainIdentifierPolicy
