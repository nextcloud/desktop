/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#import <Cocoa/Cocoa.h>
#import <QString>

#include "application.h"

namespace OCC
{

QString socketApiSocketPath()
{
    // This must match the code signing Team setting of the extension
    // Example for developer builds (with ad-hoc signing identity): "" "com.owncloud.desktopclient" ".socket"
    // Example for official signed packages: "9B5WD74GWJ." "com.owncloud.desktopclient" ".socket"
    NSString *appGroupId = @SOCKETAPI_TEAM_IDENTIFIER_PREFIX APPLICATION_REV_DOMAIN;

    NSURL *container = [[NSFileManager defaultManager] containerURLForSecurityApplicationGroupIdentifier:appGroupId];
    NSURL *socketPath = [container URLByAppendingPathComponent:@".socket" isDirectory:false];
    return QString::fromNSString(socketPath.path);
}

}
