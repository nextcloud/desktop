/*
 * Copyright (C) by Denis Dzyubenko
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


#ifndef SETTINGSDIALOGMAC_H
#define SETTINGSDIALOGMAC_H

#include "progressdispatcher.h"
#include "macpreferenceswindow.h"
#include "owncloudgui.h"

class QStandardItemModel;
class QListWidgetItem;

namespace OCC {

class AccountSettings;
class ProtocolWidget;
class Application;
class FolderMan;
class ownCloudGui;
class Folder;
class AccountState;
class ActivitySettings;

/**
 * @brief The SettingsDialogMac class
 * @ingroup gui
 */
class SettingsDialogMac : public MacPreferencesWindow
{
    Q_OBJECT

public:
    explicit SettingsDialogMac(ownCloudGui *gui, QWidget *parent = 0);

public slots:
    void showActivityPage();
    void slotRefreshActivity(AccountState *accountState );

private slots:
    void accountAdded(AccountState *);
    void accountRemoved(AccountState *);
    void slotAccountAvatarChanged();
private:
    void closeEvent(QCloseEvent *event);

    ProtocolWidget  *_protocolWidget;
    ActivitySettings *_activitySettings;
    ownCloudGui     *_gui;

    int _protocolIdx;
};

}

#endif // SETTINGSDIALOGMAC_H
