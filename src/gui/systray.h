/*
 * Copyright (C) by Cédric Bellegarde <gnumdk@gmail.com>
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

#ifndef SYSTRAY_H
#define SYSTRAY_H

#include <QSystemTrayIcon>

class QIcon;

namespace OCC {

#ifdef Q_OS_OSX
bool canOsXSendUserNotification();
void sendOsXUserNotification(const QString &title, const QString &message);
#endif

/**
 * @brief The Systray class
 * @ingroup gui
 */
class Systray : public QSystemTrayIcon
{
    Q_OBJECT
public:
    void showMessage(const QString &title, const QString &message, MessageIcon icon = Information, int millisecondsTimeoutHint = 10000);
    void setToolTip(const QString &tip);
};

} // namespace OCC

#endif //SYSTRAY_H
