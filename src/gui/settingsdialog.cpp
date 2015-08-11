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
#include "accountmanager.h"

#include <QLabel>
#include <QStandardItemModel>
#include <QStackedWidget>
#include <QPushButton>
#include <QDebug>
#include <QSettings>
#include <QToolBar>
#include <QToolButton>
#include <QLayout>
#include <QVBoxLayout>

namespace {
  const char TOOLBAR_CSS[] =
    "QToolBar { background: %1; margin: 0; padding: 0; border: none; border-bottom: 1px solid %2; spacing: 0; } "
    "QToolBar QToolButton { background: %1; border: none; border-bottom: 1px solid %2; margin: 0; padding: 0; } "
    "QToolBar QToolButton:checked { background: %3; color: %4; }";

  void addActionToToolBar(QAction *action, QToolBar *tb) {
    QToolButton* btn = new QToolButton;
    btn->setDefaultAction(action);
    btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    btn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    tb->addWidget(btn);
  }
}

namespace OCC {

//
// Whenever you change something here check both settingsdialog.cpp and settingsdialogmac.cpp !
//
SettingsDialog::SettingsDialog(ownCloudGui *gui, QWidget *parent) :
    QDialog(parent)
    , _ui(new Ui::SettingsDialog), _gui(gui)

{
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    _ui->setupUi(this);
    QToolBar *toolBar = new QToolBar;
    QString highlightColor(palette().highlight().color().name());
    QString altBase(palette().alternateBase().color().name());
    QString dark(palette().dark().color().name());
    QString background(palette().base().color().name());
    toolBar->setStyleSheet(QString::fromAscii(TOOLBAR_CSS).arg(background).arg(dark).arg(highlightColor).arg(altBase));
    toolBar->setIconSize(QSize(32, 32));
    toolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    layout()->setMenuBar(toolBar);

    // People perceive this as a Window, so also make Ctrl+W work
    QAction *closeWindowAction = new QAction(this);
    closeWindowAction->setShortcut(QKeySequence("Ctrl+W"));
    connect(closeWindowAction, SIGNAL(triggered()), SLOT(accept()));
    addAction(closeWindowAction);


    setObjectName("Settings"); // required as group for saveGeometry call
    setWindowTitle(Theme::instance()->appNameGUI());

    // Add a spacer so config buttonns are right aligned and account buttons will be left aligned
    auto spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    toolBar->addWidget(spacer);
    QActionGroup *group = new QActionGroup(this);
    group->setExclusive(true);

    // Note: all the actions have a '\n' because the account name is in two lines and
    // all buttons must have the same size in order to keep a good layout
    QIcon protocolIcon(QLatin1String(":/client/resources/activity.png"));
    _protocolAction = group->addAction(protocolIcon, tr("Activity"));
    _protocolAction->setCheckable(true);
    addActionToToolBar(_protocolAction, toolBar);
    ProtocolWidget *protocolWidget = new ProtocolWidget;
    _ui->stack->addWidget(protocolWidget);

    QIcon generalIcon(QLatin1String(":/client/resources/settings.png"));
    QAction *generalAction =  group->addAction(generalIcon, tr("General"));
    generalAction->setCheckable(true);
    addActionToToolBar(generalAction, toolBar);
    GeneralSettings *generalSettings = new GeneralSettings;
    _ui->stack->addWidget(generalSettings);

    QIcon networkIcon(QLatin1String(":/client/resources/network.png"));
    QAction *networkAction =  group->addAction(networkIcon, tr("Network"));
    networkAction->setCheckable(true);
    addActionToToolBar(networkAction, toolBar);
    NetworkSettings *networkSettings = new NetworkSettings;
    _ui->stack->addWidget(networkSettings);

    _actions.insert(_protocolAction, protocolWidget);
    _actions.insert(generalAction, generalSettings);
    _actions.insert(networkAction, networkSettings);

    connect(group, SIGNAL(triggered(QAction*)), SLOT(slotSwitchPage(QAction*)));

    connect(AccountManager::instance(), SIGNAL(accountAdded(AccountState*)),
            this, SLOT(accountAdded(AccountState*)));
    connect(AccountManager::instance(), SIGNAL(accountRemoved(AccountState*)),
            this, SLOT(accountRemoved(AccountState*)));
    foreach (auto ai , AccountManager::instance()->accounts()) {
        accountAdded(ai.data());
    }

    // default to Account
    toolBar->actions().at(0)->trigger();

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

void SettingsDialog::accountAdded(AccountState *s)
{
    QIcon accountIcon(QLatin1String(":/client/resources/account.png"));
    auto toolBar = qobject_cast<QToolBar*>(layout()->menuBar());
    Q_ASSERT(toolBar);
    auto accountAction = new QAction(accountIcon, s->shortDisplayNameForSettings(), this);
    accountAction->setToolTip(s->account()->displayName());
    accountAction->setCheckable(true);

    QToolButton* accountButton = new QToolButton;
    accountButton->setDefaultAction(accountAction);
    accountButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    accountButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    toolBar->insertWidget(toolBar->actions().at(0), accountButton);

    auto accountSettings = new AccountSettings(s, this);
    _ui->stack->insertWidget(0 , accountSettings);
    _actions.insert(accountAction, accountSettings);

    auto group = findChild<QActionGroup*>(
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
                    QString() , Qt::FindDirectChildrenOnly
#endif
        );
    Q_ASSERT(group);
    group->addAction(accountAction);

    connect( accountSettings, SIGNAL(folderChanged()), _gui, SLOT(slotFoldersChanged()));
    connect( accountSettings, SIGNAL(openFolderAlias(const QString&)),
             _gui, SLOT(slotFolderOpenAction(QString)));

}

void SettingsDialog::accountRemoved(AccountState *s)
{
    for (auto it = _actions.begin(); it != _actions.end(); ++it) {
        auto as = qobject_cast<AccountSettings *>(*it);
        if (!as) {
            continue;
        }
        if (as->accountsState() == s) {
            delete it.key();
            delete it.value();
            _actions.erase(it);
            break;
        }
    }
}



} // namespace OCC
