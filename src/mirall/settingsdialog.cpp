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

#include "settingsdialog.h"
#include "ui_settingsdialog.h"

#include "mirall/folderman.h"
#include "mirall/theme.h"
#include "mirall/generalsettings.h"
#include "mirall/networksettings.h"
#include "mirall/accountsettings.h"
#include "mirall/application.h"
#include "mirall/mirallconfigfile.h"
#include "mirall/progressdispatcher.h"

#include <QLabel>
#include <QStandardItemModel>
#include <QPushButton>
#include <QDebug>
#include <QSettings>

namespace Mirall {

QIcon createDummy() {
    QIcon icon;
    QPixmap p(32,32);
    p.fill(Qt::transparent);
    icon.addPixmap(p);
    return icon;
}

SettingsDialog::SettingsDialog(Application *app, QWidget *parent) :
    QDialog(parent),
    _ui(new Ui::SettingsDialog)
{
    _ui->setupUi(this);
    setObjectName("Settings"); // required as group for saveGeometry call

    setWindowTitle(tr("%1 Settings").arg(Theme::instance()->appNameGUI()));

    QIcon generalIcon(QLatin1String(":/mirall/resources/settings.png"));
    QListWidgetItem *general = new QListWidgetItem(generalIcon, tr("General"), _ui->labelWidget);
    general->setSizeHint(QSize(0, 32));
    _ui->labelWidget->addItem(general);
    GeneralSettings *generalSettings = new GeneralSettings;
    _ui->stack->addWidget(generalSettings);

    QIcon networkIcon(QLatin1String(":/mirall/resources/network.png"));
    QListWidgetItem *network = new QListWidgetItem(networkIcon, tr("Network"), _ui->labelWidget);
    network->setSizeHint(QSize(0, 32));
    _ui->labelWidget->addItem(network);
    NetworkSettings *networkSettings = new NetworkSettings;
    _ui->stack->addWidget(networkSettings);
    connect(networkSettings, SIGNAL(proxySettingsChanged()), app, SLOT(slotSetupProxy()));
    connect(networkSettings, SIGNAL(proxySettingsChanged()), FolderMan::instance(), SLOT(slotScheduleAllFolders()));

    //connect(generalSettings, SIGNAL(resizeToSizeHint()), SLOT(resizeToSizeHint()));

    _accountSettings = new AccountSettings(this);
    addAccount(tr("Account"), _accountSettings);
    slotUpdateAccountState();

    connect( app, SIGNAL(folderStateChanged(Folder*)), _accountSettings, SLOT(slotUpdateFolderState(Folder*)));
    connect( app, SIGNAL(folderStateChanged(Folder*)), SLOT(slotUpdateAccountState()));

    connect( _accountSettings, SIGNAL(addASync()), app, SLOT(slotFolderAdded()) );
    connect( _accountSettings, SIGNAL(folderChanged()), app, SLOT(slotFoldersChanged()));
    connect( _accountSettings, SIGNAL(openFolderAlias(const QString&)),
             app, SLOT(slotFolderOpenAction(QString)));
    connect( _accountSettings, SIGNAL(openProgressDialog()), app, SLOT(slotItemProgressDialog()));

    connect( ProgressDispatcher::instance(), SIGNAL(progressInfo(QString, Progress::Info)),
             _accountSettings, SLOT(slotSetProgress(QString, Progress::Info)) );

    _ui->labelWidget->setCurrentRow(_ui->labelWidget->row(general));

    connect(_ui->labelWidget, SIGNAL(currentRowChanged(int)),
            _ui->stack, SLOT(setCurrentIndex(int)));

    QPushButton *closeButton = _ui->buttonBox->button(QDialogButtonBox::Close);
    connect(closeButton, SIGNAL(pressed()), SLOT(accept()));

    MirallConfigFile cfg;
    cfg.restoreGeometry(this);
}

SettingsDialog::~SettingsDialog()
{
    delete _ui;
}

void SettingsDialog::addAccount(const QString &title, QWidget *widget)
{
    _accountItem = new QListWidgetItem(Theme::instance()->syncStateIcon(SyncResult::Undefined, true), title);
    _accountItem->setSizeHint(QSize(0, 32));
    _ui->labelWidget->addItem(_accountItem);
    _ui->stack->addWidget(widget);

}

void SettingsDialog::closeEvent(QCloseEvent *event)
{
    MirallConfigFile cfg;
    cfg.saveGeometry(this);
    QWidget::closeEvent(event);
}

void SettingsDialog::slotUpdateAccountState()
{
    FolderMan *folderMan = FolderMan::instance();
    SyncResult state = folderMan->accountStatus(folderMan->map().values());
    _accountItem->setIcon(Theme::instance()->syncStateIcon(state.status()));
}

} // namespace Mirall
