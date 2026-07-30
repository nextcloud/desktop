/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "findersyncbrokerregistrar.h"

#import <Foundation/Foundation.h>
#import <ServiceManagement/ServiceManagement.h>

#include <QCoreApplication>
#include <QLoggingCategory>
#include <QMetaObject>

#include "findersyncbrokeridentity.h"

namespace OCC::Mac {

Q_LOGGING_CATEGORY(lcFinderSyncBrokerRegistrar, "nextcloud.gui.macos.findersync.broker", QtInfoMsg)

namespace {

    /// The login item is addressed by its bundle identifier, which for the broker is also the
    /// Mach service name it vends.
    QString brokerIdentifier()
    {
        return FinderSyncBrokerIdentity::brokerServiceName();
    }

    SMAppService *brokerService()
    {
        return [SMAppService loginItemServiceWithIdentifier:brokerIdentifier().toNSString()];
    }

    FinderSyncBrokerRegistrar::Status toStatus(SMAppServiceStatus status)
    {
        switch (status) {
        case SMAppServiceStatusEnabled:
            return FinderSyncBrokerRegistrar::Status::Enabled;
        case SMAppServiceStatusRequiresApproval:
            return FinderSyncBrokerRegistrar::Status::RequiresApproval;
        case SMAppServiceStatusNotFound:
            return FinderSyncBrokerRegistrar::Status::NotFound;
        case SMAppServiceStatusNotRegistered:
            break;
        }

        return FinderSyncBrokerRegistrar::Status::NotRegistered;
    }

} // namespace

FinderSyncBrokerRegistrar::Status FinderSyncBrokerRegistrar::status()
{
    @autoreleasepool {
        return toStatus(brokerService().status);
    }
}

FinderSyncBrokerRegistrar::Status FinderSyncBrokerRegistrar::ensureRegistered()
{
    @autoreleasepool {
        SMAppService *service = brokerService();
        const auto current = toStatus(service.status);

        if (current == Status::Enabled) {
            qCDebug(lcFinderSyncBrokerRegistrar) << "FinderSync broker login item is already enabled";
            return current;
        }

        if (current == Status::RequiresApproval) {
            // The user's decision, not a failure to fix here. Re-registering would only earn a
            // kSMErrorLaunchDeniedByUser.
            qCWarning(lcFinderSyncBrokerRegistrar)
                << "FinderSync broker login item is switched off in System Settings; "
                   "Finder badges and the context menu will not work until it is enabled";
            return current;
        }

        NSError *error = nil;
        const bool registered = [service registerAndReturnError:&error];

        // Registering something already registered is not a problem, it is the steady state.
        if (!registered && error.code != kSMErrorAlreadyRegistered) {
            qCCritical(lcFinderSyncBrokerRegistrar)
                << "Could not register the FinderSync broker login item:"
                << QString::fromNSString(error.localizedDescription)
                << "- Finder badges and the context menu will not work";

            return toStatus(service.status);
        }

        // Re-read rather than assuming success means Enabled: a register can land in
        // RequiresApproval, and a NotFound that survives a successful-looking register means the
        // bundle cannot be resolved at all — a packaging fault, not something to retry.
        const auto result = toStatus(service.status);

        switch (result) {
        case Status::Enabled:
            qCInfo(lcFinderSyncBrokerRegistrar) << "FinderSync broker login item is registered and enabled";
            break;
        case Status::RequiresApproval:
            qCWarning(lcFinderSyncBrokerRegistrar)
                << "FinderSync broker login item was registered but needs approval in System Settings";
            break;
        case Status::NotFound:
            qCCritical(lcFinderSyncBrokerRegistrar)
                << "FinderSync broker login item is unknown to the system after registering:"
                << brokerIdentifier()
                << "- the login item bundle is missing, or its CFBundleIdentifier does not match "
                   "its wrapper filename, so SMAppService cannot resolve it";
            break;
        case Status::NotRegistered:
            qCCritical(lcFinderSyncBrokerRegistrar)
                << "FinderSync broker login item still reports as not registered after registering";
            break;
        }

        return result;
    }
}

void FinderSyncBrokerRegistrar::reregisterAsync(std::function<void(Status)> completion)
{
    @autoreleasepool {
        SMAppService *service = brokerService();
        const auto current = toStatus(service.status);

        if (current == Status::RequiresApproval) {
            // Never unregister an item the user switched off. The unregister may well succeed and
            // erase their explicit decision, after which we cannot put it back and they get
            // re-prompted for something they already declined.
            qCWarning(lcFinderSyncBrokerRegistrar)
                << "Not restarting the FinderSync broker login item: it is switched off in "
                   "System Settings, which is the user's decision to reverse";
            completion(current);
            return;
        }

        qCInfo(lcFinderSyncBrokerRegistrar) << "Restarting the FinderSync broker login item";

        [service unregisterWithCompletionHandler:^(NSError *error) {
            if (error) {
                qCWarning(lcFinderSyncBrokerRegistrar)
                    << "Could not unregister the FinderSync broker login item:"
                    << QString::fromNSString(error.localizedDescription);
            }

            // The completion handler runs on an arbitrary queue, and registering has to happen
            // where the rest of the login item handling does. Hop to the main thread via qApp,
            // which lives there.
            QMetaObject::invokeMethod(qApp, [completion] {
                completion(ensureRegistered());
            }, Qt::QueuedConnection);
        }];
    }
}

void FinderSyncBrokerRegistrar::openLoginItemsSettings()
{
    @autoreleasepool {
        [SMAppService openSystemSettingsLoginItems];
    }
}

QString FinderSyncBrokerRegistrar::describe(Status status)
{
    switch (status) {
    case Status::Enabled:
        return QStringLiteral("enabled");
    case Status::RequiresApproval:
        return QStringLiteral("switched off in System Settings");
    case Status::NotFound:
        return QStringLiteral("unknown to the system");
    case Status::NotRegistered:
        return QStringLiteral("not registered");
    }

    return QStringLiteral("unknown");
}

} // namespace OCC::Mac
