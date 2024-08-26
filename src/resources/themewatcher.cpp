/*
 * Copyright (C) by Hannah von Reth <hannah.vonreth@owncloud.com>
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

#include "themewatcher.h"

#include <QCoreApplication>
#include <QEvent>

namespace OCC {
namespace Resources {
    ThemeWatcher::ThemeWatcher(QObject *parent)
        : QObject(parent)
    {
        qApp->installEventFilter(this);
    }

    bool ThemeWatcher::eventFilter(QObject *watched, QEvent *event)
    {
        if (event->type() == QEvent::ThemeChange) {
            Q_EMIT themeChanged();
        }
        return QObject::eventFilter(watched, event);
    }
} // Resources
} // OCC
