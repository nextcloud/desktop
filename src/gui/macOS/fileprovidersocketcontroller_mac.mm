/*
 * Copyright (C) 2022 by Claudio Cambra <claudio.cambra@nextcloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#import <Cocoa/Cocoa.h>
#import <QString>

#include "config.h"

namespace OCC
{

namespace Mac
{

QString fileProviderSocketPath()
{
    // This must match the code signing Team setting of the extension
    // Example for developer builds (with ad-hoc signing identity): "" "com.owncloud.desktopclient" ".fileprovidersocket"
    // Example for official signed packages: "9B5WD74GWJ." "com.owncloud.desktopclient" ".fileprovidersocket"
    NSString *appGroupId = @SOCKETAPI_TEAM_IDENTIFIER_PREFIX APPLICATION_REV_DOMAIN;

    NSURL *container = [[NSFileManager defaultManager] containerURLForSecurityApplicationGroupIdentifier:appGroupId];
    NSURL *socketPath = [container URLByAppendingPathComponent:@".fileprovidersocket" isDirectory:false];
    return QString::fromNSString(socketPath.path);
}

} // namespace Mac

} // namespace OCC
