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

    // Under MRC, block arguments may be stack blocks. Copy to heap before capturing
    // in the lambda, which executes asynchronously after this method returns.
    // Do NOT autorelease: the autorelease pool drains on the XPC thread before the
    // lambda fires on Qt's main thread, which would leave a dangling pointer.
    auto copiedHandler = [completionHandler copy];

    // Use QMetaObject::invokeMethod to call on the correct thread
    QMetaObject::invokeMethod(socketApi, [service = _service, qPath, copiedHandler]() {
        auto handler = (void(^)(NSString *, NSError *))copiedHandler;
        // Get file status via FinderSyncService helper (which has friend access to FileData)
        const auto [hasFolder, statusString] = service->getFileStatus(qPath);

        if (!hasFolder) {
            qCDebug(OCC::lcMacFinderSyncService) << "File not in any sync folder:" << qPath;
            handler(@"NOP", nil);
            [copiedHandler release];
            return;
        }

        qCDebug(OCC::lcMacFinderSyncService) << "Returning file status:" << statusString << "for:" << qPath;
        handler(statusString.toNSString(), nil);
        [copiedHandler release];
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
    completionHandler([[strings copy] autorelease], nil);
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

    // Store socketApi pointer after null check to prevent TOCTOU race
    auto *socketApi = _service ? _service->socketApi() : nullptr;
    if (!socketApi) {
        NSError *error = [NSError errorWithDomain:@"com.nextcloud.desktopclient.FinderSyncService"
                                             code:1
                                         userInfo:@{NSLocalizedDescriptionKey: @"SocketApi not available"}];
        completionHandler(nil, error);
        return;
    }

    // Convert NSArray to QStringList
    QStringList qPaths;
    for (NSString *path in paths) {
        qPaths << QString::fromNSString(path);
    }

    // Under MRC, block arguments may be stack blocks. Copy to heap before capturing
    // in the lambda, which executes asynchronously after this method returns.
    // Do NOT autorelease: the autorelease pool drains on the XPC thread before the
    // lambda fires on Qt's main thread, which would leave a dangling pointer.
    auto copiedHandler = [completionHandler copy];

    // Marshal to SocketApi's thread — command handlers touch core Qt/FolderMan state
    QMetaObject::invokeMethod(socketApi, [service = _service, qPaths, copiedHandler]() {
        auto handler = (void(^)(NSArray<NSDictionary *> *, NSError *))copiedHandler;
        // Get menu items from SocketApi via FinderSyncService helper
        const auto qMenuItems = service->getMenuItems(qPaths);

        // Convert QList<QMap> to NSArray<NSDictionary>
        NSMutableArray *menuItems = [NSMutableArray array];
        for (const auto &qItem : qMenuItems) {
            NSMutableDictionary *item = [NSMutableDictionary dictionary];
            for (auto it = qItem.constBegin(); it != qItem.constEnd(); ++it) {
                item[it.key().toNSString()] = it.value().toNSString();
            }
            [menuItems addObject:[[item copy] autorelease]];
        }

        qCDebug(OCC::lcMacFinderSyncService) << "Returning" << menuItems.count << "menu items";
        handler([[menuItems copy] autorelease], nil);
        [copiedHandler release];
    }, Qt::QueuedConnection);
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

    // Whitelist of commands the FinderSync extension is allowed to invoke.
    // This prevents untrusted XPC clients from calling arbitrary SocketApi methods.
    static const QSet<QString> allowedCommands = {
        QStringLiteral("SHARE"),
        QStringLiteral("LEAVESHARE"),
        QStringLiteral("COPY_SECUREFILEDROP_LINK"),
        QStringLiteral("COPY_PRIVATE_LINK"),
        QStringLiteral("EMAIL_PRIVATE_LINK"),
        QStringLiteral("OPEN_PRIVATE_LINK"),
        QStringLiteral("MAKE_AVAILABLE_LOCALLY"),
        QStringLiteral("MAKE_ONLINE_ONLY"),
        QStringLiteral("RESOLVE_CONFLICT"),
        QStringLiteral("DELETE_ITEM"),
        QStringLiteral("MOVE_ITEM"),
        QStringLiteral("LOCK_FILE"),
        QStringLiteral("UNLOCK_FILE"),
        QStringLiteral("ACTIVITY"),
        QStringLiteral("ENCRYPT"),
        QStringLiteral("COPYASPATH"),
        QStringLiteral("EDIT"),
    };

    if (!allowedCommands.contains(qCommand)) {
        qCWarning(OCC::lcMacFinderSyncService) << "Rejected disallowed command:" << qCommand;
        NSError *error = [NSError errorWithDomain:@"com.nextcloud.desktopclient.FinderSyncService"
                                             code:5
                                         userInfo:@{NSLocalizedDescriptionKey: @"Command not allowed"}];
        completionHandler(error);
        return;
    }

    // Under MRC, block arguments may be stack blocks. Copy to heap before capturing
    // in the lambda, which executes asynchronously after this method returns.
    // Do NOT autorelease: the autorelease pool drains on the XPC thread before the
    // lambda fires on Qt's main thread, which would leave a dangling pointer.
    auto copiedHandler = [completionHandler copy];

    // Use QMetaObject::invokeMethod to execute the command on SocketApi's thread
    QMetaObject::invokeMethod(socketApi, [socketApi, qCommand, qPaths, copiedHandler]() {
        auto handler = (void(^)(NSError *))copiedHandler;
        // Build the command method name (e.g., "SHARE" -> "command_SHARE")
        const QString methodName = QStringLiteral("command_%1").arg(qCommand);

        // Create a null listener since we're not sending responses back via socket
        // Commands will execute but won't send socket responses
        OCC::SocketListener *nullListener = nullptr;

        // Join all paths with record separator (same encoding as the socket protocol)
        // to preserve multi-selection for commands like MAKE_AVAILABLE_LOCALLY
        const QString argument = qPaths.join(QChar(0x1e));

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
            handler(nil);
        } else {
            qCWarning(OCC::lcMacFinderSyncService) << "Command execution failed (method not found):" << qCommand;
            NSError *error = [NSError errorWithDomain:@"com.nextcloud.desktopclient.FinderSyncService"
                                                 code:4
                                             userInfo:@{NSLocalizedDescriptionKey: @"Command method not found"}];
            handler(error);
        }
        [copiedHandler release];
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

    // Use the canonical toSocketAPIString() to avoid duplicating status mapping logic
    const auto statusString = fileData.syncFileStatus().toSocketAPIString();
    return {true, statusString};
}

/**
 * @brief SocketListener subclass that captures command responses in-memory.
 *
 * SocketApi command methods (e.g., command_GET_STRINGS, command_GET_MENU_ITEMS)
 * produce their output by calling listener->sendMessage(). This design dates
 * back to the UNIX-socket transport where responses were written directly to
 * the socket.
 *
 * With the XPC transport there is no socket, yet we still need to invoke these
 * commands and collect their output. ResponseCapturingListener acts as an
 * in-memory adapter: it receives the sendMessage() calls and stores them in a
 * QStringList that the caller can parse and convert into XPC reply values.
 *
 * This is production code, not a test helper.
 */
namespace {
class ResponseCapturingListener : public SocketListener
{
public:
    mutable QStringList messages;

    explicit ResponseCapturingListener()
        : SocketListener(nullptr)
    {
    }

    void sendMessage(const QString &message, bool = false) const override
    {
        messages.append(message);
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
    ResponseCapturingListener listener;

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
            // Parse "STRING:KEY:VALUE" - use KeepEmptyParts to preserve empty values
            const auto remainder = msg.mid(7); // Skip "STRING:"
            const auto colonPos = remainder.indexOf(':');
            if (colonPos > 0) {
                const QString key = remainder.left(colonPos);
                const QString value = remainder.mid(colonPos + 1);
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
    ResponseCapturingListener listener;

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


