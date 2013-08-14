/*
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#ifndef MIRALL_GENERALSETTINGS_H
#define MIRALL_GENERALSETTINGS_H

#include <QWidget>


namespace Mirall {

namespace Ui {
class GeneralSettings;
}

class GeneralSettings : public QWidget
{
    Q_OBJECT

public:
    explicit GeneralSettings(QWidget *parent = 0);
    ~GeneralSettings();

signals:
    void proxySettingsChanged();

private slots:
    void saveMiscSettings();
    void slotToggleLaunchOnStartup(bool);
    void slotToggleOptionalDesktopNotifications(bool);

private:
    void loadMiscSettings();

    Ui::GeneralSettings *_ui;
};


} // namespace Mirall
#endif // MIRALL_GENERALSETTINGS_H
