/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QString>

#include <functional>

namespace OCC::Mac {

/**
 * @brief Registers the FinderSync broker login item with Service Management.
 *
 * The broker has to be a launchd job, because only a launchd job may advertise the Mach
 * service name that the FinderSync extension looks up. Registering it is what turns the
 * bundle in Contents/Library/LoginItems into such a job.
 *
 * Registration state lives in the system's Background Task Management database, not in our
 * config: it survives app updates, uninstalls and reboots, and the user (or an MDM profile)
 * can switch the item off in System Settings. So this is not a one-time setup step — it has to
 * be reconciled on every launch, and its status has to be reported rather than assumed.
 */
class FinderSyncBrokerRegistrar
{
public:
    /// Mirrors SMAppServiceStatus.
    enum class Status {
        /// Never registered, or unregistered again.
        NotRegistered,
        /// Registered and eligible to run.
        Enabled,
        /// Registered, but switched off by the user or an MDM profile in System Settings.
        RequiresApproval,
        /// The system has no record of this service at all.
        NotFound,
    };

    [[nodiscard]] static Status status();

    /**
     * @brief Register the login item if it is not already registered.
     *
     * Idempotent: an already-registered service reports kSMErrorAlreadyRegistered, which is
     * treated as success. Does not attempt to override a user who has switched the item off —
     * re-registering in that state fails with kSMErrorLaunchDeniedByUser, so RequiresApproval
     * is reported instead and left for the user to resolve.
     *
     * @return The status **re-read after** the attempt, not inferred from whether the register
     * call returned successfully. A register can legitimately land in RequiresApproval, and
     * reporting that as success would have callers wait forever for a broker that is switched
     * off, with nothing in the log to explain it.
     */
    static Status ensureRegistered();

    /**
     * @brief Unregister and re-register, without blocking the caller.
     *
     * Needed after an update: the Background Task Management entry survives replacing the app
     * bundle, but an already-running broker keeps executing the old binary, so it has to be
     * restarted deliberately. Re-registering before the unregistration has completed fails with
     * SMAppServiceErrorDomain code 1, so the two cannot simply be called in sequence.
     *
     * Asynchronous on purpose. Awaiting the unregistration on the calling thread means blocking
     * the GUI thread for as long as it takes, which users experience as a hang; and once more
     * than one call site exists, serializing them by blocking is the wrong tool.
     *
     * @param completion Invoked on the main thread with the status after the attempt. Not
     * invoked before this function returns.
     */
    static void reregisterAsync(std::function<void(Status)> completion);

    /// Open System Settings at the Login Items pane, for the RequiresApproval case.
    static void openLoginItemsSettings();

    /// Human-readable description of @p status, for logs and the settings UI.
    [[nodiscard]] static QString describe(Status status);
};

} // namespace OCC::Mac
