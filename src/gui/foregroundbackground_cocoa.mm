/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "foregroundbackground_cocoa.h"
#include "common/utility.h"

@implementation CocoaProcessType

+ (void)ToForeground
{
        NSApplicationLoad();
        ProcessSerialNumber processSerialNumber = { 0, kCurrentProcess };
        TransformProcessType(&processSerialNumber, kProcessTransformToForegroundApplication);
}

+ (void)ToBackground
{
        NSApplicationLoad();
        ProcessSerialNumber processSerialNumber = { 0, kCurrentProcess };
        TransformProcessType(&processSerialNumber, kProcessTransformToUIElementApplication);
}

@end