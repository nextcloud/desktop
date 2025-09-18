/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "foregroundbackground_interface.h"
#include "foregroundbackground_cocoa.h"

bool ForegroundBackground::eventFilter(QObject * /*obj*/, QEvent *event)
{
    if (event->type() == QEvent::Show) {
        ToForeground();
        return true;
    }
    if (event->type() == QEvent::Close) {
        ToBackground();
        return true;
    }
    return false;
}

void ForegroundBackground::ToForeground()
{
    [CocoaProcessType ToForeground];
}

void ForegroundBackground::ToBackground()
{
    [CocoaProcessType ToBackground];
}
