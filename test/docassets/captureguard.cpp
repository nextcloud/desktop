/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "captureguard.h"
#include <QEvent>
namespace OCC::DocAssets
{
bool CaptureGuard::eventFilter(QObject *, QEvent *event)
{
    switch (event->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseButtonDblClick:
    case QEvent::KeyPress:
    case QEvent::KeyRelease:
    case QEvent::Wheel:
    case QEvent::TouchBegin:
    case QEvent::FileOpen:
        return true;
    default:
        return false;
    }
}
void CaptureGuard::consumeUrl(const QUrl &url)
{
    Q_UNUSED(url)
}
}
