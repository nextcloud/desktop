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
    NSString *appGroupId = [NSString stringWithFormat:@"group.%@", @APPLICATION_REV_DOMAIN];
    NSURL *container = [[NSFileManager defaultManager] containerURLForSecurityApplicationGroupIdentifier:appGroupId];
    NSURL *socketPath = [container URLByAppendingPathComponent:@".socket" isDirectory:false];
    return QString::fromNSString(socketPath.path);
}

}
