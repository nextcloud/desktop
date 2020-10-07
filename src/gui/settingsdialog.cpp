/*
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
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

#include "settingsdialog.h"
#include "ui_settingsdialog.h"

#include "application.h"
#include "folderman.h"
#include "theme.h"
#include "generalsettings.h"
#include "guiutility.h"
#include "networksettings.h"
#include "accountsettings.h"
#include "configfile.h"
#include "progressdispatcher.h"
#include "owncloudgui.h"
#include "activitywidget.h"
#include "accountmanager.h"
#include "protocolwidget.h"
#include "owncloudsetupwizard.h"

#include <QLabel>
#include <QStandardItemModel>
#include <QStackedWidget>
#include <QPushButton>
#include <QSettings>
#include <QToolBar>
#include <QToolButton>
#include <QLayout>
#include <QVBoxLayout>
#include <QPixmap>
#include <QImage>
#include <QWidgetAction>
#include <QPainter>
#include <QPainterPath>
#include <QMessageBox>

#ifdef Q_OS_MAC
#include "settingsdialog_mac.h"

void setActivationPolicy(ActivationPolicy policy);
#endif

namespace {
const QString TOOLBAR_CSS()
{
    return QStringLiteral("QToolBar { background: %1; margin: 0; padding: 0; border: none; border-bottom: 1px solid %2; spacing: 0; } "
                          "QToolBar QToolButton { background: %1; border: none; border-bottom: 1px solid %2; margin: 0; padding: 5px; } "
                          "QToolBar QToolBarExtension { padding:0; } "
                          "QToolBar QToolButton:checked { background: %3; color: %4; }");
}

const float BUTTONSIZERATIO = 1.618f; // golden ratio


/** display name with two lines that is displayed in the settings
 */
QString shortDisplayNameForSettings(OCC::Account *account)
{
    QString user = account->davDisplayName();
    if (user.isEmpty()) {
        user = account->credentials()->user();
    }
    QString host = account->url().host();
    int port = account->url().port();
    if (port > 0 && port != 80 && port != 443) {
        host.append(QLatin1Char(':'));
        host.append(QString::number(port));
    }
    return QStringLiteral("%1\n%2").arg(user, host);
}
}


namespace OCC {

SettingsDialog::SettingsDialog(ownCloudGui *gui, QWidget *parent)
    : QMainWindow(parent, Qt::WindowFlags() & ~Qt::WindowContextHelpButtonHint)
    , _ui(new Ui::SettingsDialog)
    , _gui(gui)
{
    ConfigFile cfg;
    _ui->setupUi(this);

    // People perceive this as a Window, so also make Ctrl+W work
    QAction *closeWindowAction = new QAction(this);
    closeWindowAction->setShortcut(QKeySequence("Ctrl+W"));
    connect(closeWindowAction, &QAction::triggered, this, &SettingsDialog::hide);
    addAction(closeWindowAction);

    setObjectName("Settings"); // required as group for saveGeometry call
    setWindowTitle(Theme::instance()->appNameGUI());

    _actionGroup = new QActionGroup(this);
    _actionGroup->setExclusive(true);


    if (Theme::instance()->multiAccount()) {
        _addAccountAction = createActionWithIcon(QIcon(QStringLiteral(":/client/resources/plus-solid.svg")), tr("Add account"));
        _addAccountAction->setCheckable(false);
        connect(_addAccountAction, &QAction::triggered, this, []{
            // don't directly connect here, ocApp might not be defined yet
            ocApp()->gui()->runNewAccountWizard();
        });
        _ui->toolBar->addAction(_addAccountAction);
    }

    // Note: all the actions have a '\n' because the account name is in two lines and
    // all buttons must have the same size in order to keep a good layout
    _activityAction = createActionWithIcon(QIcon(QStringLiteral(":/client/resources/activity.svg")), tr("Activity"));
    _actionGroup->addAction(_activityAction);
    _ui->toolBar->addAction(_activityAction);
    _activitySettings = new ActivitySettings;
    _ui->stack->addWidget(_activitySettings);
    connect(_activitySettings, &ActivitySettings::guiLog, _gui,
        &ownCloudGui::slotShowOptionalTrayMessage);
    _activitySettings->setNotificationRefreshInterval(cfg.notificationRefreshInterval());

    QAction *generalAction = createActionWithIcon(QIcon(QStringLiteral(":/client/resources/settings.svg")), tr("Settings"));
    _actionGroup->addAction(generalAction);
    _ui->toolBar->addAction(generalAction);
    GeneralSettings *generalSettings = new GeneralSettings;
    _ui->stack->addWidget(generalSettings);
    QObject::connect(generalSettings, &GeneralSettings::showAbout, gui, &ownCloudGui::slotAbout);

    QWidget *spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
    _ui->toolBar->addWidget(spacer);

    QAction *quitAction = createActionWithIcon(QIcon(QStringLiteral(":/client/resources/quit.svg")), tr("Quit %1").arg(qApp->applicationName()));
    quitAction->setCheckable(false);
    connect(quitAction, &QAction::triggered, this, [this] {
        const auto reply = QMessageBox::question(this, tr("Quit %1").arg(qApp->applicationName()),
            tr("Are you sure you want to quit %1?").arg(qApp->applicationName()),
            QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            qApp->quit();
        }
    });
    _ui->toolBar->addAction(quitAction);

    _actionGroupWidgets.insert(_activityAction, _activitySettings);
    _actionGroupWidgets.insert(generalAction, generalSettings);

    connect(_actionGroup, &QActionGroup::triggered, this, &SettingsDialog::slotSwitchPage);

    connect(AccountManager::instance(), &AccountManager::accountAdded,
        this, &SettingsDialog::accountAdded);
    connect(AccountManager::instance(), &AccountManager::accountRemoved,
        this, &SettingsDialog::accountRemoved);
    foreach (auto ai, AccountManager::instance()->accounts()) {
        accountAdded(ai.data());
    }

    QTimer::singleShot(1, this, &SettingsDialog::showFirstPage);

    connect(_ui->hideButton, &QPushButton::clicked, this, &SettingsDialog::hide);

    QAction *showLogWindow = new QAction(this);
    showLogWindow->setShortcut(QKeySequence("F12"));
    connect(showLogWindow, &QAction::triggered, gui, &ownCloudGui::slotToggleLogBrowser);
    addAction(showLogWindow);

    QAction *showLogWindow2 = new QAction(this);
    showLogWindow2->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_L));
    connect(showLogWindow2, &QAction::triggered, gui, &ownCloudGui::slotToggleLogBrowser);
    addAction(showLogWindow2);

    customizeStyle();

    cfg.restoreGeometry(this);
    if (size().width() < minimumSizeHint().width() || size().height() < minimumSizeHint().height())
    {
        resize(minimumSizeHint());
    }

#ifdef Q_OS_MAC
    setActivationPolicy(ActivationPolicy::Accessory);
#endif
}

SettingsDialog::~SettingsDialog()
{
    delete _ui;
}

QSize SettingsDialog::minimumSizeHint() const
{
    return {800, 500};
}

QWidget* SettingsDialog::currentPage()
{
    return _ui->stack->currentWidget();
}


void SettingsDialog::changeEvent(QEvent *e)
{
    switch (e->type()) {
    case QEvent::StyleChange:
    case QEvent::PaletteChange:
    case QEvent::ThemeChange:
        customizeStyle();
        break;
    default:
        break;
    }

    QMainWindow::changeEvent(e);
}

void SettingsDialog::setVisible(bool visible)
{
    if (!visible)
    {
        ConfigFile cfg;
        cfg.saveGeometry(this);
    }

#ifdef Q_OS_MAC
    if (visible) {
        setActivationPolicy(ActivationPolicy::Regular);
    } else {
        setActivationPolicy(ActivationPolicy::Accessory);
    }
#endif
    QMainWindow::setVisible(visible);
}

void SettingsDialog::slotSwitchPage(QAction *action)
{
    _ui->stack->setCurrentWidget(_actionGroupWidgets.value(action));
}

void SettingsDialog::showFirstPage()
{
    const QList<QAction *> &actions = _ui->toolBar->actions();
    if (!actions.empty()) {
        actions.at(_addAccountAction ? 1 : 0)->trigger();
    }
}

void SettingsDialog::showActivityPage()
{
    if (_activityAction) {
        _activityAction->trigger();
    }
}

void SettingsDialog::showIssuesList(const QString &folderAlias)
{
    if (!_activityAction)
        return;
    _activityAction->trigger();
    _activitySettings->slotShowIssuesTab(folderAlias);
}

void SettingsDialog::accountAdded(AccountState *s)
{
    bool brandingSingleAccount = !Theme::instance()->multiAccount();

    QAction *accountAction;
    QImage avatar = s->account()->avatar();
    const QString actionText = brandingSingleAccount ? tr("Account") : s->account()->displayName();
    if (avatar.isNull()) {
        accountAction = createActionWithIcon(QIcon(QStringLiteral(":/client/resources/account.svg")),
            actionText);
    } else {
        QIcon icon(QPixmap::fromImage(AvatarJob::makeCircularAvatar(avatar)));
        accountAction = createActionWithIcon(icon, actionText);
    }

    if (!brandingSingleAccount) {
        accountAction->setToolTip(s->account()->displayName());
        accountAction->setIconText(shortDisplayNameForSettings(s->account().data()));
    }
    _ui->toolBar->insertAction(_addAccountAction ? _ui->toolBar->actions().at(1) : _ui->toolBar->actions().at(0), accountAction);
    auto accountSettings = new AccountSettings(s, this);
    QString objectName = QLatin1String("accountSettings_");
    objectName += s->account()->displayName();
    accountSettings->setObjectName(objectName);
    _ui->stack->insertWidget(0 , accountSettings);

    _actionGroup->addAction(accountAction);
    _actionGroupWidgets.insert(accountAction, accountSettings);
    _actionForAccount.insert(s->account().data(), accountAction);
    accountAction->trigger();

    connect(accountSettings, &AccountSettings::folderChanged, _gui, &ownCloudGui::slotFoldersChanged);
    connect(accountSettings, &AccountSettings::openFolderAlias,
        _gui, &ownCloudGui::slotFolderOpenAction);
    connect(accountSettings, &AccountSettings::showIssuesList, this, &SettingsDialog::showIssuesList);
    connect(s->account().data(), &Account::accountChangedAvatar, this, &SettingsDialog::slotAccountAvatarChanged);
    connect(s->account().data(), &Account::accountChangedDisplayName, this, &SettingsDialog::slotAccountDisplayNameChanged);

    // Refresh immediatly when getting online
    connect(s, &AccountState::isConnectedChanged, this, &SettingsDialog::slotRefreshActivityAccountStateSender);

    slotRefreshActivity(s);
}

void SettingsDialog::slotAccountAvatarChanged()
{
    Account *account = static_cast<Account *>(sender());
    if (account && _actionForAccount.contains(account)) {
        QAction *action = _actionForAccount[account];
        if (action) {
            QImage pix = account->avatar();
            if (!pix.isNull()) {
                action->setIcon(QPixmap::fromImage(AvatarJob::makeCircularAvatar(pix)));
            }
        }
    }
}

void SettingsDialog::slotAccountDisplayNameChanged()
{
    Account *account = static_cast<Account *>(sender());
    if (account && _actionForAccount.contains(account)) {
        QAction *action = _actionForAccount[account];
        if (action) {
            QString displayName = account->displayName();
            action->setText(displayName);
            action->setIconText(shortDisplayNameForSettings(account));
        }
    }
}

void SettingsDialog::accountRemoved(AccountState *s)
{
    for (auto it = _actionGroupWidgets.begin(); it != _actionGroupWidgets.end(); ++it) {
        auto as = qobject_cast<AccountSettings *>(*it);
        if (!as) {
            continue;
        }
        if (as->accountsState() == s) {
            _ui->toolBar->removeAction(it.key());

            if (_ui->stack->currentWidget() == it.value()) {
                showFirstPage();
            }

            it.key()->deleteLater();
            it.value()->deleteLater();
            _actionGroupWidgets.erase(it);
            break;
        }
    }

    if (_actionForAccount.contains(s->account().data())) {
        _actionForAccount.remove(s->account().data());
    }
    _activitySettings->slotRemoveAccount(s);
}

void SettingsDialog::customizeStyle()
{
    QString highlightColor(palette().highlight().color().name());
    QString highlightTextColor(palette().highlightedText().color().name());
    QString dark(palette().dark().color().name());
    QString background(palette().base().color().name());
    _ui->toolBar->setStyleSheet(TOOLBAR_CSS().arg(background, dark, highlightColor, highlightTextColor));
}

class ToolButtonAction : public QWidgetAction
{
public:
    explicit ToolButtonAction(const QIcon &icon, const QString &text, QObject *parent)
        : QWidgetAction(parent)
    {
        setText(text);
        setIcon(icon);
    }


    QWidget *createWidget(QWidget *parent) override
    {
        auto toolbar = qobject_cast<QToolBar *>(parent);
        if (!toolbar) {
            // this means we are in the extention menu, no special action here
            return nullptr;
        }

        QToolButton *btn = new QToolButton(toolbar);
        QString objectName = QLatin1String("settingsdialog_toolbutton_");
        objectName += text();
        btn->setObjectName(objectName);

        btn->setDefaultAction(this);
        btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        btn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::MinimumExpanding);
        // icon size is fixed, we can't use the toolbars actual size hint as it might not be defined yet
        btn->setMinimumWidth(toolbar->iconSize().height() * BUTTONSIZERATIO);
        return btn;
    }
};

QAction *SettingsDialog::createActionWithIcon(const QIcon &icon, const QString &text)
{
    QAction *action = new ToolButtonAction(icon, text, this);
    action->setCheckable(true);
    return action;
}

void SettingsDialog::slotRefreshActivityAccountStateSender()
{
    slotRefreshActivity(qobject_cast<AccountState*>(sender()));
}

void SettingsDialog::slotRefreshActivity(AccountState *accountState)
{
    if (accountState) {
        _activitySettings->slotRefresh(accountState);
    }
}

} // namespace OCC
