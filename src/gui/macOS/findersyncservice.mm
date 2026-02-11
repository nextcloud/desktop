/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "findersyncservice.h"

#import <Foundation/Foundation.h>
#import "FinderSyncAppProtocol.h"

#include <QLoggingCategory>
#include <QMetaObject>
#include <QBuffer>

// Note: SocketApi is NOT socket-specific - it's the core business logic class that handles
// file status queries, menu commands, and sync operations across ALL platforms.
// We reuse SocketApi here as a transport adapter to avoid duplicating business logic.
#include "socketapi/socketapi.h"
#include "socketapi/socketapi_p.h"
#include "common/syncfilestatus.h"

namespace OCC {

Q_LOGGING_CATEGORY(lcMacFinderSyncService, "nextcloud.gui.macfindersyncservice", QtInfoMsg)

} // namespace OCC

/**
 * @brief Objective-C delegate that implements the FinderSyncAppProtocol.
 */
@interface FinderSyncServiceDelegate : NSObject<FinderSyncAppProtocol>
@property (nonatomic, assign) OCC::Mac::FinderSyncService *service;
@end

@implementation FinderSyncServiceDelegate

- (void)retrieveFileStatusForPath:(NSString *)path
                completionHandler:(void(^)(NSString *status, NSError *error))completionHandler
{
    const auto qPath = QString::fromNSString(path);
    qCDebug(OCC::lcMacFinderSyncService) << "FinderSync requesting file status for:" << qPath;

    // Store socketApi pointer after null check to prevent TOCTOU race
    auto *socketApi = _service ? _service->socketApi() : nullptr;
    if (!socketApi) {
        NSError *error = [NSError errorWithDomain:@"com.nextcloud.desktopclient.FinderSyncService"
                                             code:1
                                         userInfo:@{NSLocalizedDescriptionKey: @"SocketApi not available"}];
        completionHandler(nil, error);
        return;
    }

    // Use QMetaObject::invokeMethod to call on the correct thread
    QMetaObject::invokeMethod(socketApi, [service = _service, qPath, completionHandler]() {
        // Get file status via FinderSyncService helper (which has friend access to FileData)
        const auto [hasFolder, statusString] = service->getFileStatus(qPath);

        if (!hasFolder) {
            qCWarning(OCC::lcMacFinderSyncService) << "File not in any sync folder:" << qPath;
            NSError *error = [NSError errorWithDomain:@"com.nextcloud.desktopclient.FinderSyncService"
                                                 code:2
                                             userInfo:@{NSLocalizedDescriptionKey: @"File not in sync folder"}];
            completionHandler(nil, error);
            return;
        }

        qCDebug(OCC::lcMacFinderSyncService) << "Returning file status:" << statusString << "for:" << qPath;
        completionHandler(statusString.toNSString(), nil);
    }, Qt::QueuedConnection);
}

- (void)retrieveFolderStatusForPath:(NSString *)path
                  completionHandler:(void(^)(NSString *status, NSError *error))completionHandler
{
    // Folders and files use the same status logic
    [self retrieveFileStatusForPath:path completionHandler:completionHandler];
}

- (void)getLocalizedStringsWithCompletionHandler:(void(^)(NSDictionary<NSString *, NSString *> *strings, NSError *error))completionHandler
{
    qCDebug(OCC::lcMacFinderSyncService) << "FinderSync requesting localized strings";

    if (!_service) {
        NSError *error = [NSError errorWithDomain:@"com.nextcloud.desktopclient.FinderSyncService"
                                             code:1
                                         userInfo:@{NSLocalizedDescriptionKey: @"Service not available"}];
        completionHandler(nil, error);
        return;
    }

    // Get strings from SocketApi via FinderSyncService helper
    const auto qStrings = _service->getLocalizedStrings();

    // Convert QMap to NSDictionary
    NSMutableDictionary *strings = [NSMutableDictionary dictionary];
    for (auto it = qStrings.constBegin(); it != qStrings.constEnd(); ++it) {
        strings[it.key().toNSString()] = it.value().toNSString();
    }

    qCDebug(OCC::lcMacFinderSyncService) << "Returning" << strings.count << "localized strings";
    completionHandler([strings copy], nil);
}

- (void)getMenuItemsForPaths:(NSArray<NSString *> *)paths
           completionHandler:(void(^)(NSArray<NSDictionary *> *menuItems, NSError *error))completionHandler
{
    if (!paths || paths.count == 0) {
        NSError *error = [NSError errorWithDomain:@"com.nextcloud.desktopclient.FinderSyncService"
                                             code:3
                                         userInfo:@{NSLocalizedDescriptionKey: @"No paths provided"}];
        completionHandler(nil, error);
        return;
    }

    qCDebug(OCC::lcMacFinderSyncService) << "FinderSync requesting menu items for" << paths.count << "paths";

    if (!_service) {
        NSError *error = [NSError errorWithDomain:@"com.nextcloud.desktopclient.FinderSyncService"
                                             code:1
                                         userInfo:@{NSLocalizedDescriptionKey: @"Service not available"}];
        completionHandler(nil, error);
        return;
    }

    // Convert NSArray to QStringList
    QStringList qPaths;
    for (NSString *path in paths) {
        qPaths << QString::fromNSString(path);
    }

    // Get menu items from SocketApi via FinderSyncService helper
    const auto qMenuItems = _service->getMenuItems(qPaths);

    // Convert QList<QMap> to NSArray<NSDictionary>
    NSMutableArray *menuItems = [NSMutableArray array];
    for (const auto &qItem : qMenuItems) {
        NSMutableDictionary *item = [NSMutableDictionary dictionary];
        for (auto it = qItem.constBegin(); it != qItem.constEnd(); ++it) {
            item[it.key().toNSString()] = it.value().toNSString();
        }
        [menuItems addObject:[item copy]];
    }

    qCDebug(OCC::lcMacFinderSyncService) << "Returning" << menuItems.count << "menu items";
    completionHandler([menuItems copy], nil);
}

- (void)executeMenuCommand:(NSString *)command
                  forPaths:(NSArray<NSString *> *)paths
         completionHandler:(void(^)(NSError *error))completionHandler
{
    const auto qCommand = QString::fromNSString(command);

    QStringList qPaths;
    for (NSString *path in paths) {
        qPaths << QString::fromNSString(path);
    }

    qCDebug(OCC::lcMacFinderSyncService) << "FinderSync executing command:" << qCommand
                                         << "for" << qPaths.size() << "paths";

    // Store socketApi pointer after null check to prevent TOCTOU race
    auto *socketApi = _service ? _service->socketApi() : nullptr;
    if (!socketApi) {
        NSError *error = [NSError errorWithDomain:@"com.nextcloud.desktopclient.FinderSyncService"
                                             code:1
                                         userInfo:@{NSLocalizedDescriptionKey: @"SocketApi not available"}];
        completionHandler(error);
        return;
    }

    // Use QMetaObject::invokeMethod to execute the command on SocketApi's thread
    QMetaObject::invokeMethod(socketApi, [socketApi, qCommand, qPaths, completionHandler]() {
        // Build the command method name (e.g., "SHARE" -> "command_SHARE")
        const QString methodName = QStringLiteral("command_%1").arg(qCommand);

        // Create a null listener since we're not sending responses back via socket
        // Commands will execute but won't send socket responses
        OCC::SocketListener *nullListener = nullptr;

        // Use the first path as the argument (commands typically operate on the first selected file)
        const QString argument = qPaths.isEmpty() ? QString() : qPaths.first();

        // Try to invoke the command using Qt's meta-object system
        bool invoked = QMetaObject::invokeMethod(
            socketApi,
            methodName.toUtf8().constData(),
            Qt::DirectConnection,
            Q_ARG(QString, argument),
            Q_ARG(OCC::SocketListener*, nullListener)
        );

        if (invoked) {
            qCDebug(OCC::lcMacFinderSyncService) << "Command executed successfully:" << qCommand;
            completionHandler(nil);
        } else {
            qCWarning(OCC::lcMacFinderSyncService) << "Command execution failed (method not found):" << qCommand;
            NSError *error = [NSError errorWithDomain:@"com.nextcloud.desktopclient.FinderSyncService"
                                                 code:4
                                             userInfo:@{NSLocalizedDescriptionKey: @"Command method not found"}];
            completionHandler(error);
        }
    }, Qt::QueuedConnection);
}

@end

namespace OCC {

namespace Mac {

class FinderSyncService::MacImplementation
{
public:
    FinderSyncServiceDelegate *delegate = nil;

    MacImplementation()
    {
        qCDebug(lcMacFinderSyncService) << "Initializing finder sync service";
    }

    ~MacImplementation()
    {
        [delegate release];
        delegate = nil;
    }
};

FinderSyncService::FinderSyncService(QObject *parent)
    : QObject(parent)
    , d(std::make_unique<MacImplementation>())
{
    qCDebug(lcMacFinderSyncService) << "FinderSyncService created";

    d->delegate = [[FinderSyncServiceDelegate alloc] init];
    d->delegate.service = this;
}

FinderSyncService::~FinderSyncService()
{
    qCDebug(lcMacFinderSyncService) << "FinderSyncService destroyed";
}

void *FinderSyncService::delegate() const
{
    return d ? d->delegate : nullptr;
}

void FinderSyncService::setSocketApi(SocketApi *socketApi)
{
    _socketApi = socketApi;
    qCDebug(lcMacFinderSyncService) << "SocketApi set for FinderSyncService";
}

SocketApi *FinderSyncService::socketApi() const
{
    return _socketApi;
}

std::pair<bool, QString> FinderSyncService::getFileStatus(const QString &path) const
{
    if (!_socketApi) {
        return {false, QStringLiteral("NOP")};
    }

    // Access private SocketApi::FileData (allowed via friend declaration)
    auto fileData = SocketApi::FileData::get(path);

    if (!fileData.folder) {
        return {false, QStringLiteral("NOP")};
    }

    const auto status = fileData.syncFileStatus();

    // Convert SyncFileStatus to string using the same logic as socket broadcasts
    QString statusString;
    if (status.tag() == SyncFileStatus::StatusSync) {
        statusString = QStringLiteral("SYNC");
    } else if (status.tag() == SyncFileStatus::StatusUpToDate) {
        statusString = status.shared() ? QStringLiteral("OK+SWM") : QStringLiteral("OK");
    } else if (status.tag() == SyncFileStatus::StatusWarning) {
        statusString = QStringLiteral("IGNORE");
    } else if (status.tag() == SyncFileStatus::StatusError) {
        statusString = QStringLiteral("ERROR");
    } else {
        statusString = QStringLiteral("NOP");
    }

    return {true, statusString};
}

// Mock SocketListener for capturing command responses
namespace {
class MockSocketListener : public SocketListener
{
public:
    QStringList messages;

    explicit MockSocketListener()
        : SocketListener(nullptr)
    {
    }

    void sendMessage(const QString &message, bool = false) const override
    {
        // Cast away const to store messages (this is a mock for testing)
        const_cast<MockSocketListener*>(this)->messages.append(message);
    }
};
} // anonymous namespace

QMap<QString, QString> FinderSyncService::getLocalizedStrings() const
{
    QMap<QString, QString> result;

    if (!_socketApi) {
        return result;
    }

    // Create mock listener to capture responses
    MockSocketListener listener;

    // Call SocketApi command (synchronous)
    _socketApi->command_GET_STRINGS(QString(), &listener);

    // Parse responses
    // Format: STRING:KEY:VALUE between GET_STRINGS:BEGIN and GET_STRINGS:END
    bool inStrings = false;
    for (const QString &msg : listener.messages) {
        if (msg == QStringLiteral("GET_STRINGS:BEGIN")) {
            inStrings = true;
        } else if (msg == QStringLiteral("GET_STRINGS:END")) {
            break;
        } else if (inStrings && msg.startsWith(QStringLiteral("STRING:"))) {
            // Parse "STRING:KEY:VALUE"
            const auto parts = msg.mid(7).split(':', Qt::SkipEmptyParts); // Skip "STRING:"
            if (parts.size() >= 2) {
                const QString key = parts[0];
                const QString value = parts.mid(1).join(':'); // Rejoin in case value contains ':'
                result[key] = value;
            }
        }
    }

    return result;
}

QList<QMap<QString, QString>> FinderSyncService::getMenuItems(const QStringList &paths) const
{
    QList<QMap<QString, QString>> result;

    if (!_socketApi || paths.isEmpty()) {
        return result;
    }

    // Create mock listener to capture responses
    MockSocketListener listener;

    // Join paths with record separator (same as socket protocol)
    const QString argument = paths.join(QChar(0x1e));

    // Call SocketApi command (synchronous)
    _socketApi->command_GET_MENU_ITEMS(argument, &listener);

    // Parse responses
    // Format: MENU_ITEM:command:flags:text between GET_MENU_ITEMS:BEGIN and GET_MENU_ITEMS:END
    bool inMenuItems = false;
    for (const QString &msg : listener.messages) {
        if (msg == QStringLiteral("GET_MENU_ITEMS:BEGIN")) {
            inMenuItems = true;
        } else if (msg == QStringLiteral("GET_MENU_ITEMS:END")) {
            break;
        } else if (inMenuItems && msg.startsWith(QStringLiteral("MENU_ITEM:"))) {
            // Parse "MENU_ITEM:command:flags:text"
            const auto parts = msg.mid(10).split(':', Qt::KeepEmptyParts); // Skip "MENU_ITEM:"
            if (parts.size() >= 3) {
                QMap<QString, QString> item;
                item[QStringLiteral("command")] = parts[0];
                item[QStringLiteral("flags")] = parts[1];
                item[QStringLiteral("text")] = parts.mid(2).join(':'); // Rejoin in case text contains ':'
                result.append(item);
            }
        }
    }

    return result;
}

} // namespace Mac

} // namespace OCC
