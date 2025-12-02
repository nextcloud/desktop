/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#import "MainAppService.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcMainAppService, "nextcloud.gui.macos.mainappservice", QtInfoMsg)

@implementation MainAppService

+ (instancetype)sharedInstance
{
    static MainAppService *service = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        service = [[MainAppService alloc] init];
    });
    return service;
}

 - (void)reportStatusForDomain:(NSFileProviderDomainIdentifier)domainIdentifier status:(NSString *)status
{
    if (domainIdentifier == nil || status == nil) {
        qCWarning(lcMainAppService) << "Received nil domain or status from File Provider";
        return;
    }
    qCInfo(lcMainAppService) << "File Provider status"
                             << domainIdentifier.UTF8String
                             << ":"
                             << status.UTF8String;
}

@end