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

#include "folderman.h"
#include "theme.h"
#include "generalsettings.h"
#include "networksettings.h"
#include "accountsettings.h"
#include "configfile.h"
#include "progressdispatcher.h"
#include "owncloudgui.h"
#include "protocolwidget.h"

#include <QLabel>
#include <QStandardItemModel>
#include <QStackedWidget>
#include <QPushButton>
#include <QDebug>
#include <QSettings>
#include <QToolBar>
#include <QLayout>

namespace {
  const char TOOLBAR_CSS[] =
    "QToolBar { background: white; margin: 0; padding: 0; border: none; border-bottom: 1px solid grey; spacing: 0; } "
    "QToolBar QToolButton { background: white; border: none; border-bottom: 1px solid grey; margin: 0; padding: 0; } "
    "QToolBar QToolButton:checked { background: %1; color: %2; }";
}

namespace OCC {

SettingsDialog::SettingsDialog(ownCloudGui *gui, QWidget *parent) :
    QDialog(parent)
    , _ui(new Ui::SettingsDialog)
    , _accountSettings(new AccountSettings)

{
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    _ui->setupUi(this);
    QToolBar *toolBar = new QToolBar;
    toolBar->setIconSize(QSize(32,32));
    QString highlightColor(palette().highlight().color().name());
    QString altBase(palette().alternateBase().color().name());
    toolBar->setStyleSheet(QString::fromAscii(TOOLBAR_CSS).arg(highlightColor).arg(altBase));
    toolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    layout()->setMenuBar(toolBar);

    // People perceive this as a Window, so also make Ctrl+W work
    QAction *closeWindowAction = new QAction(this);
    closeWindowAction->setShortcut(QKeySequence("Ctrl+W"));
    connect(closeWindowAction, SIGNAL(triggered()), SLOT(accept()));
    addAction(closeWindowAction);

    setObjectName("Settings"); // required as group for saveGeometry call

    setWindowTitle(Theme::instance()->appNameGUI());

    QIcon accountIcon(QLatin1String(":/client/resources/accounts.png"));
    QAction *accountAction = toolBar->addAction(accountIcon, tr("Account"));
    accountAction->setCheckable(true);
    _ui->stack->addWidget(_accountSettings);

    QIcon protocolIcon(QLatin1String(":/client/resources/activity.png"));
    _protocolAction = toolBar->addAction(protocolIcon, tr("Activity"));
    _protocolAction->setCheckable(true);
    ProtocolWidget *protocolWidget = new ProtocolWidget;
    _ui->stack->addWidget(protocolWidget);

    QIcon generalIcon(QLatin1String(":/client/resources/settings.png"));
    QAction *generalAction = toolBar->addAction(generalIcon, tr("General"));
    generalAction->setCheckable(true);
    GeneralSettings *generalSettings = new GeneralSettings;
    _ui->stack->addWidget(generalSettings);

    QIcon networkIcon(QLatin1String(":/client/resources/network.png"));
    QAction *networkAction = toolBar->addAction(networkIcon, tr("Network"));
    networkAction->setCheckable(true);
    NetworkSettings *networkSettings = new NetworkSettings;
    _ui->stack->addWidget(networkSettings);

    _actions.insert(accountAction, _accountSettings);
    _actions.insert(_protocolAction, protocolWidget);
    _actions.insert(generalAction, generalSettings);
    _actions.insert(networkAction, networkSettings);

    QActionGroup *group = new QActionGroup(this);
    group->addAction(accountAction);
    group->addAction(_protocolAction);
    group->addAction(generalAction);
    group->addAction(networkAction);
    group->setExclusive(true);
    connect(group, SIGNAL(triggered(QAction*)), SLOT(slotSwitchPage(QAction*)));

    connect( _accountSettings, SIGNAL(folderChanged()), gui, SLOT(slotFoldersChanged()));
    connect( _accountSettings, SIGNAL(openFolderAlias(const QString&)),
             gui, SLOT(slotFolderOpenAction(QString)));

    connect( ProgressDispatcher::instance(), SIGNAL(progressInfo(QString, Progress::Info)),
             _accountSettings, SLOT(slotSetProgress(QString, Progress::Info)) );


    // default to Account
    accountAction->setChecked(true);

    QPushButton *closeButton = _ui->buttonBox->button(QDialogButtonBox::Close);
    connect(closeButton, SIGNAL(clicked()), SLOT(accept()));

    QAction *showLogWindow = new QAction(this);
    showLogWindow->setShortcut(QKeySequence("F12"));
    connect(showLogWindow, SIGNAL(triggered()), gui, SLOT(slotToggleLogBrowser()));
    addAction(showLogWindow);

    ConfigFile cfg;
    cfg.restoreGeometry(this);
}

SettingsDialog::~SettingsDialog()
{
    delete _ui;
}

void SettingsDialog::setGeneralErrors(const QStringList &errors)
{
    _accountSettings->setGeneralErrors(errors);
}

// close event is not being called here
void SettingsDialog::reject() {
    ConfigFile cfg;
    cfg.saveGeometry(this);
    QDialog::reject();
}

void SettingsDialog::accept() {
    ConfigFile cfg;
    cfg.saveGeometry(this);
    QDialog::accept();
}

void SettingsDialog::slotSwitchPage(QAction *action)
{
    _ui->stack->setCurrentWidget(_actions.value(action));
}

void SettingsDialog::showActivityPage()
{
    if (_protocolAction) {
        slotSwitchPage(_protocolAction);
    }
}


} // namespace OCC
