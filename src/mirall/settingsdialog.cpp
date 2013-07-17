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

#include "mirall/theme.h"
#include "mirall/generalsettings.h"
#include "mirall/accountsettings.h"
#include "mirall/application.h"
#include "mirall/ignorelisteditor.h"
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

    setWindowTitle(tr("%1 Settings").arg(Theme::instance()->appNameGUI()));

    QListWidgetItem *general = new QListWidgetItem(tr("General Settings"), _ui->labelWidget);
    general->setSizeHint(QSize(0, 32));
    _ui->labelWidget->addItem(general);
    GeneralSettings *generalSettings = new GeneralSettings;
    connect(generalSettings, SIGNAL(proxySettingsChanged()), app, SLOT(slotSetupProxy()));
    connect(generalSettings, SIGNAL(proxySettingsChanged()), app->_folderMan, SLOT(slotScheduleAllFolders()));
    _ui->stack->addWidget(generalSettings);

    QListWidgetItem *ignoredFiles = new QListWidgetItem(tr("Ignored Files"), _ui->labelWidget);
    ignoredFiles->setSizeHint(QSize(0, 32));
    _ui->labelWidget->addItem(ignoredFiles);
    IgnoreListEditor *ignoreEditor = new IgnoreListEditor;
    _ui->stack->addWidget(ignoreEditor);

    //connect(generalSettings, SIGNAL(resizeToSizeHint()), SLOT(resizeToSizeHint()));

    _accountSettings = new AccountSettings(app->_folderMan);
    addAccount(tr("Account Details"), _accountSettings);

    connect( app, SIGNAL(folderStateChanged(Folder*)), _accountSettings, SLOT(slotUpdateFolderState(Folder*)));

    connect( _accountSettings, SIGNAL(addASync()), app, SLOT(slotFolderAdded()) );
    connect( _accountSettings, SIGNAL(folderChanged()), app, SLOT(slotFoldersChanged()));
    connect( _accountSettings, SIGNAL(openFolderAlias(const QString&)),
             app, SLOT(slotFolderOpenAction(QString)));

    connect( ProgressDispatcher::instance(), SIGNAL(folderProgress(Progress::Kind, QString,QString,long,long)),
             _accountSettings, SLOT(slotSetProgress(Progress::Kind, QString,QString,long,long)));

    connect(ProgressDispatcher::instance(), SIGNAL(shortFolderProgress(QString, QString)),
            this, SLOT(slotShortFolderProgress(QString, QString)));

    _ui->labelWidget->setCurrentRow(_ui->labelWidget->row(general));

    connect(_ui->labelWidget, SIGNAL(currentRowChanged(int)),
            _ui->stack, SLOT(setCurrentIndex(int)));

    QPushButton *closeButton = _ui->buttonBox->button(QDialogButtonBox::Close);
    connect(closeButton, SIGNAL(pressed()), SLOT(done()));

    MirallConfigFile cfg;
    QSettings settings(cfg.configFile(), QSettings::IniFormat);
    restoreGeometry(settings.value("Settings/geometry").toByteArray());
}

SettingsDialog::~SettingsDialog()
{
    delete _ui;
}

void SettingsDialog::addAccount(const QString &title, QWidget *widget)
{
    QListWidgetItem *item = new QListWidgetItem(title);
    item->setSizeHint(QSize(0, 32));
    _ui->labelWidget->addItem(item);
    _ui->stack->addWidget(widget);

}

void SettingsDialog::closeEvent(QCloseEvent *event)
{
    MirallConfigFile cfg;
    QSettings settings(cfg.configFile(), QSettings::IniFormat);
    settings.setValue("Settings/geometry", saveGeometry());
    QWidget::closeEvent(event);
}

void SettingsDialog::done()
{
    QDialog::done(0);
}

} // namespace Mirall
