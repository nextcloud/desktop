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

class SettingsDialogMac : public MacPreferencesWindow
{
    Q_OBJECT

public:
    explicit SettingsDialogMac(ownCloudGui *gui, QWidget *parent = 0);

    void setGeneralErrors( const QStringList& errors );

public slots:
    void slotSyncStateChange(const QString& alias);
    void showActivityPage();

private:
    void closeEvent(QCloseEvent *event);

    AccountSettings *_accountSettings;
    QListWidgetItem *_accountItem;
    ProtocolWidget  *_protocolWidget;

    int _protocolIdx;
};

}

#endif // SETTINGSDIALOGMAC_H
