/*
 * Copyright (C) by Denis Dzyubenko
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


#ifndef SETTINGSDIALOGMAC_H
#define SETTINGSDIALOGMAC_H

#include "progressdispatcher.h"
#include "macpreferenceswindow.h"

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

private slots:
    void accountAdded(AccountState *);
    void accountRemoved(AccountState *);

private:
    void closeEvent(QCloseEvent *event);

    QListWidgetItem *_accountItem;
    ProtocolWidget  *_protocolWidget;
    ownCloudGui     *_gui;

    int _protocolIdx;
};

}

#endif // SETTINGSDIALOGMAC_H
