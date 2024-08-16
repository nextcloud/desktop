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

#include "accountmanager.h"
#include "accountsettings.h"
#include "activitywidget.h"
#include "application.h"
#include "configfile.h"
#include "generalsettings.h"
#include "gui/qmlutils.h"
#include "owncloudgui.h"
#include "resources/qmlresources.h"
#include "resources/resources.h"
#include "theme.h"

#include <QScreen>
#include <QWindow>

#ifdef Q_OS_MAC
#include "settingsdialog_mac.h"

void setActivationPolicy(ActivationPolicy policy);
#endif

Q_LOGGING_CATEGORY(lcSettingsDialog, "gui.settingsdialog", QtInfoMsg);

namespace {
auto minimumSizeHint(const QWidget *w)
{
    const QSize min { 800, 700 }; // When changing this, please check macOS: widgets there have larger insets, so they take up more space.
    const auto screen = w->windowHandle() ? w->windowHandle()->screen() : QApplication::screenAt(QCursor::pos());
    if (screen) {
        const auto availableSize = screen->availableSize();
        if (availableSize.isValid()) {
            // Assume we can use at least 90% of the screen, if the screen is smaller than 800x700 pixels.
            //
            // Note: this means that the wizards have even less space: with the style we use, the
            // wizard tries to fit inside the window. So, if this is a common case that users have
            // such small screens, and the contents of the wizard screen are squashed together (or
            // not shown due to lack of space), we should consider putting that content in a
            // scroll-view.
            return min.boundedTo(availableSize * 0.9);
        }
    }
    return min;
}


class AvatarImageProvider : public QQuickImageProvider
{
    Q_OBJECT
public:
    AvatarImageProvider()
        : QQuickImageProvider(QQuickImageProvider::Pixmap, QQuickImageProvider::ForceAsynchronousImageLoading)
    {
    }

    QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize)
    {
        const auto qmlIcon = OCC::Resources::QMLResources::parseIcon(id);
        const auto accountState = OCC::AccountManager::instance()->account(QUuid::fromString(qmlIcon.iconName));
        return OCC::Resources::pixmap(requestedSize, accountState->account()->avatar(), qmlIcon.enabled ? QIcon::Normal : QIcon::Disabled, size);
    }
};
}


namespace OCC {

SettingsDialog::SettingsDialog(ownCloudGui *gui, QWidget *parent)
    : QMainWindow(parent)
    , _ui(new Ui::SettingsDialog)
    , _gui(gui)
{
    setObjectName(QStringLiteral("Settings")); // required as group for saveGeometry call
    setWindowTitle(Theme::instance()->appNameGUI());
    _ui->setupUi(this);

    setMinimumSize(::minimumSizeHint(this));

    // People perceive this as a Window, so also make Ctrl+W work
    addAction(tr("Hide"), Qt::CTRL | Qt::Key_W, this, &SettingsDialog::hide);

    // TODO: fix sizing
    _ui->quickWidget->setFixedHeight(minimumHeight() * 0.15);
    _ui->quickWidget->engine()->addImageProvider(QStringLiteral("avatar"), new AvatarImageProvider);
    QmlUtils::initQuickWidget(_ui->quickWidget, QUrl(QStringLiteral("qrc:/qt/qml/org/ownCloud/gui/qml/AccountBar.qml")), this);
    connect(
        _ui->quickWidget->engine(), &QQmlEngine::quit, QApplication::instance(),
        [this] {
            auto box = new QMessageBox(QMessageBox::Question, tr("Quit %1").arg(Theme::instance()->appNameGUI()),
                tr("Are you sure you want to quit %1?").arg(Theme::instance()->appNameGUI()), QMessageBox::Yes | QMessageBox::No, this);
            box->setAttribute(Qt::WA_DeleteOnClose);
            connect(box, &QMessageBox::accepted, this, [] {
                // delay quit to prevent a Qt 6.6 crash in the destructor of the dialog
                QTimer::singleShot(0, qApp, &QCoreApplication::quit);
            });
            box->open();
        },
        Qt::QueuedConnection);

    _activitySettings = new ActivitySettings;
    _ui->stack->addWidget(_activitySettings);
    connect(_activitySettings, &ActivitySettings::guiLog, _gui,
        [this](const QString &title, const QString &msg) {
            _gui->slotShowOptionalTrayMessage(title, msg);
        });
    ConfigFile cfg;
    _activitySettings->setNotificationRefreshInterval(cfg.notificationRefreshInterval());

    _generalSettings = new GeneralSettings;
    _ui->stack->addWidget(_generalSettings);
    connect(_generalSettings, &GeneralSettings::showAbout, gui, &ownCloudGui::slotAbout);
    connect(_generalSettings, &GeneralSettings::syncOptionsChanged, FolderMan::instance(), &FolderMan::slotReloadSyncOptions);

    cfg.restoreGeometry(this);
#ifdef Q_OS_MAC
    setActivationPolicy(ActivationPolicy::Accessory);
#endif

    connect(_ui->dialogStack, &QStackedWidget::currentChanged, this, [this] {
        auto *w = _ui->dialogStack->currentWidget();
        if (!w->windowTitle().isEmpty()) {
            setWindowTitle(tr("%1 - %2").arg(Theme::instance()->appNameGUI(), w->windowTitle()));
        } else {
            setWindowTitle(Theme::instance()->appNameGUI());
        }
    });

    setCurrentPage(SettingsPage::Settings);
    auto addAccount = [this](AccountStatePtr accountStatePtr) {
        auto accountSettings = new AccountSettings(accountStatePtr, this);
        _ui->stack->addWidget(accountSettings);
        _widgetForAccount.insert(accountStatePtr->account().data(), accountSettings);
        // select the first added account
        if (_widgetForAccount.size() == 1) {
            setCurrentAccount(accountStatePtr->account().data());
        }
    };
    for (const auto &accountState : AccountManager::instance()->accounts()) {
        addAccount(accountState);
    }
    connect(AccountManager::instance(), &AccountManager::accountAdded, this, addAccount);
    connect(AccountManager::instance(), &AccountManager::accountRemoved, this, [this](AccountStatePtr accountStatePtr) {
        _ui->stack->removeWidget(accountSettings(accountStatePtr->account().data()));
        // go to the settings page if the last account was removed
        if (_widgetForAccount.isEmpty()) {
            _ui->stack->setCurrentWidget(_generalSettings);
        }
    });
}

SettingsDialog::~SettingsDialog()
{
    delete _ui;
}

void SettingsDialog::addModalWidget(QWidget *w)
{
    ownCloudGui::raise();
    if (_ui->dialogStack->indexOf(w) == -1) {
        _ui->dialogStack->addWidget(w);
        _ui->dialogStack->setCurrentWidget(w);
    }
}

void SettingsDialog::requestModality(Account *account)
{
    _ui->quickWidget->setEnabled(false);
    if (_modalStack.isEmpty()) {
        setCurrentAccount(account);
    }
    _modalStack.append(account);
    ownCloudGui::raise();
}

void SettingsDialog::ceaseModality(Account *account)
{
    if (_modalStack.contains(account)) {
        _modalStack.removeOne(account);
        if (!_modalStack.isEmpty()) {
            setCurrentAccount(_modalStack.first());
        }
    }
    _ui->quickWidget->setEnabled(_modalStack.isEmpty());
}

AccountSettings *SettingsDialog::accountSettings(Account *account) const
{
    return _widgetForAccount.value(account, nullptr);
}

void SettingsDialog::setVisible(bool visible)
{
    if (!visible) {
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

void SettingsDialog::setCurrentPage(SettingsPage currentPage)
{
    _currentPage = currentPage;
    _currentAccount = nullptr;
    switch (_currentPage) {
    case SettingsPage::Activity:
        _ui->stack->setCurrentWidget(_activitySettings);
        break;
    case SettingsPage::Settings:
        _ui->stack->setCurrentWidget(_generalSettings);
        break;
    case SettingsPage::Account:
        // handled by set account
        [[fallthrough]];
    case SettingsPage::None:
        Q_UNREACHABLE();
    }
    Q_EMIT currentAccountChanged();
    Q_EMIT currentPageChanged();
}

SettingsDialog::SettingsPage SettingsDialog::currentPage() const
{
    return _currentPage;
}


void SettingsDialog::setCurrentAccount(Account *account)
{
    _currentAccount = account;
    _ui->stack->setCurrentWidget(accountSettings(account));
    _currentPage = SettingsPage::Account;

    Q_EMIT currentAccountChanged();
    Q_EMIT currentPageChanged();
}

Account *SettingsDialog::currentAccount() const
{
    return _currentAccount;
}

void SettingsDialog::addAccount()
{
    ocApp()->gui()->runNewAccountWizard();
}

} // namespace OCC

#include "settingsdialog.moc"
