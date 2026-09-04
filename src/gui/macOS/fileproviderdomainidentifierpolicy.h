/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QSet>
#include <QString>

namespace OCC::Mac::FileProviderDomainIdentifierPolicy {

/**
 * @brief What `addFileProviderDomain` should do after listing the domains macOS currently has.
 */
enum class RegistrationAction {
    Abort, //!< Domain listing failed; do not add or mint a new identifier.
    Skip, //!< The stored identifier is already registered.
    AddStored, //!< Re-register using the stored identifier (do not mint a new UUID).
    AddFresh, //!< No stored identifier; mint a new UUID and register it.
};

/**
 * @brief Result of `decideRegistration`.
 */
struct RegistrationDecision
{
    RegistrationAction action = RegistrationAction::Abort;
};

/**
 * @brief Decide how to register a file provider domain for an account.
 *
 * Minting a new UUID when the account already has an identifier is what produces a second
 * Finder location (`… (date)`) after a listing failure or after the user removes the domain
 * in System Settings. Re-adding with the same identifier is the File Provider contract for
 * replacing a registration in place.
 *
 * @param storedIdentifier The account's persisted domain identifier, or empty if none.
 * @param registeredIdentifiers Identifiers currently returned by `NSFileProviderManager`.
 * @param domainListingSucceeded `false` when the system listing call reported an error; the
 *        registered set must then be treated as unknown, not empty.
 */
[[nodiscard]] RegistrationDecision decideRegistration(const QString &storedIdentifier,
                                                      const QSet<QString> &registeredIdentifiers,
                                                      bool domainListingSucceeded);

} // namespace OCC::Mac::FileProviderDomainIdentifierPolicy
