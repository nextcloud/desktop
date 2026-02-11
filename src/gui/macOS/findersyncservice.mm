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

#include "socketapi/socketapi.h"
#include "socketapi/socketapi_p.h"
#include "common/syncfilestatus.h"

namespace OCC {

Q_LOGGING_CATEGORY(lcMacFinderSyncService, "nextcloud.gui.macfindersyncservice", QtInfoMsg)

} // namespace OCC

/**
 * @brief Simple QIODevice wrapper for XPC callbacks.
 *
 * This allows us to reuse SocketApi's existing command infrastructure for XPC.
 */
@interface XPCCallbackDevice : NSObject
@property (nonatomic, strong) NSMutableString *responseBuffer;
@property (nonatomic, copy) void(^completionHandler)(NSString *response, NSError *error);

- (instancetype)initWithCompletionHandler:(void(^)(NSString *response, NSError *error))handler;
- (QIODevice *)createQIODevice;
@end

@implementation XPCCallbackDevice

- (instancetype)initWithCompletionHandler:(void(^)(NSString *response, NSError *error))handler
{
    self = [super init];
    if (self) {
        _responseBuffer = [NSMutableString string];
        _completionHandler = handler;
    }
    return self;
}

- (QIODevice *)createQIODevice
{
    // Create a QBuffer that we can write to
    auto buffer = new QBuffer();
    buffer->open(QIODevice::ReadWrite);
    return buffer;
}

@end

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

    if (!_service || !_service->socketApi()) {
        NSError *error = [NSError errorWithDomain:@"com.nextcloud.desktopclient.FinderSyncService"
                                             code:1
                                         userInfo:@{NSLocalizedDescriptionKey: @"SocketApi not available"}];
        completionHandler(nil, error);
        return;
    }

    // Use QMetaObject::invokeMethod to call on the correct thread
    QMetaObject::invokeMethod(_service->socketApi(), [this, qPath, completionHandler]() {
        // Get file status via SocketApi's FileData helper
        auto fileData = OCC::SocketApi::FileData::get(qPath);
        if (!fileData.folder) {
            qCWarning(OCC::lcMacFinderSyncService) << "File not in any sync folder:" << qPath;
            NSError *error = [NSError errorWithDomain:@"com.nextcloud.desktopclient.FinderSyncService"
                                                 code:2
                                             userInfo:@{NSLocalizedDescriptionKey: @"File not in sync folder"}];
            completionHandler(nil, error);
            return;
        }

        const auto status = fileData.syncFileStatus();

        // Convert SyncFileStatus to string using the same logic as socket broadcasts
        QString statusString;
        if (status.tag() == OCC::SyncFileStatus::StatusSync) {
            statusString = QStringLiteral("SYNC");
        } else if (status.tag() == OCC::SyncFileStatus::StatusUpToDate) {
            statusString = status.shared() ? QStringLiteral("OK+SWM") : QStringLiteral("OK");
        } else if (status.tag() == OCC::SyncFileStatus::StatusWarning) {
            statusString = QStringLiteral("IGNORE");
        } else if (status.tag() == OCC::SyncFileStatus::StatusError) {
            statusString = QStringLiteral("ERROR");
        } else {
            statusString = QStringLiteral("NOP");
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

    // For now, return a basic set of strings
    // TODO: Hook this up to SocketApi's GET_STRINGS command properly
    NSDictionary *strings = @{
        @"CONTEXT_MENU_TITLE": @"Nextcloud",
        @"SHARE_MENU_TITLE": @"Share…"
    };

    qCDebug(OCC::lcMacFinderSyncService) << "Returning" << strings.count << "localized strings";
    completionHandler(strings, nil);
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

    if (!_service || !_service->socketApi()) {
        NSError *error = [NSError errorWithDomain:@"com.nextcloud.desktopclient.FinderSyncService"
                                             code:1
                                         userInfo:@{NSLocalizedDescriptionKey: @"SocketApi not available"}];
        completionHandler(nil, error);
        return;
    }

    // For now, return a basic menu structure
    // TODO: Hook this up to SocketApi's GET_MENU_ITEMS command properly with a mock listener
    NSMutableArray *menuItems = [NSMutableArray array];

    [menuItems addObject:@{
        @"command": @"ACTIVITY",
        @"flags": @"",
        @"text": @"Activity"
    }];

    [menuItems addObject:@{
        @"command": @"SHARE",
        @"flags": @"",
        @"text": @"Share…"
    }];

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

    if (!_service || !_service->socketApi()) {
        NSError *error = [NSError errorWithDomain:@"com.nextcloud.desktopclient.FinderSyncService"
                                             code:1
                                         userInfo:@{NSLocalizedDescriptionKey: @"SocketApi not available"}];
        completionHandler(error);
        return;
    }

    // Use QMetaObject::invokeMethod to execute the command on SocketApi's thread
    QMetaObject::invokeMethod(_service->socketApi(), [this, qCommand, qPaths, completionHandler]() {
        // Build the command method name (e.g., "SHARE" -> "command_SHARE")
        const QString methodName = QStringLiteral("command_%1").arg(qCommand);

        // Create a null listener since we're not sending responses back via socket
        // Commands will execute but won't send socket responses
        OCC::SocketListener *nullListener = nullptr;

        // Use the first path as the argument (commands typically operate on the first selected file)
        const QString argument = qPaths.isEmpty() ? QString() : qPaths.first();

        // Try to invoke the command using Qt's meta-object system
        const auto metaObject = _service->socketApi()->metaObject();
        bool invoked = QMetaObject::invokeMethod(
            _service->socketApi(),
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

} // namespace Mac

} // namespace OCC
