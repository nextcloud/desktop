/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2013 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "settingsdialog.h"

#include "advancedsettings.h"
#include "folderman.h"
#include "theme.h"
#include "generalsettings.h"
#include "infosettings.h"
#include "networksettings.h"
#include "accountsettings.h"
#include "configfile.h"
#include "progressdispatcher.h"
#include "owncloudgui.h"
#include "accountmanager.h"

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
#include <QPaintEvent>
#include <QShowEvent>
#include <QPalette>
#include <QQuickView>
#include <QActionGroup>
#include <QScopedValueRollback>
#include <QScrollArea>
#include <QSizePolicy>
#include <QStyle>
#include <QStyleOptionToolButton>
#include <QTimer>
#include <QMouseEvent>
#include <QWindow>
#include <QtGlobal>
#include <QScreen>
#include <QGuiApplication>

#ifdef Q_OS_MACOS
#include "nativetitlebar_mac.h"
#endif

using namespace Qt::StringLiterals;

#ifdef Q_OS_WIN
    // "light" looks too bright on dark mode on Windows only
    #define BACKGROUND_PALETTE "alternate-base"
#else
    // ...and "alternate-base" looks too bright on macOS only.  On Linux/Plasma either one looked fine ...
    #define BACKGROUND_PALETTE "light"
#endif

namespace {
class CurrentPageSizeStackedWidget : public QStackedWidget
{
public:
    using QStackedWidget::QStackedWidget;

    [[nodiscard]] QSize sizeHint() const override
    {
        if (const auto *widget = currentWidget()) {
            return widget->sizeHint();
        }
        return QStackedWidget::sizeHint();
    }

    [[nodiscard]] QSize minimumSizeHint() const override
    {
        if (const auto *widget = currentWidget()) {
            return widget->minimumSizeHint();
        }
        return QStackedWidget::minimumSizeHint();
    }

    [[nodiscard]] bool hasHeightForWidth() const override
    {
        if (const auto *widget = currentWidget()) {
            return widget->hasHeightForWidth();
        }
        return QStackedWidget::hasHeightForWidth();
    }

    [[nodiscard]] int heightForWidth(int width) const override
    {
        if (const auto *widget = currentWidget()) {
            return widget->hasHeightForWidth() ? widget->heightForWidth(width) : widget->sizeHint().height();
        }
        return QStackedWidget::heightForWidth(width);
    }

};

constexpr auto TOOLBAR_CSS = QLatin1String(
    "QToolBar { background: transparent; margin: 0; padding: 0; border: none; spacing: 0; } "
    "QToolBar QToolButton { background: transparent; border: none; margin: 0; padding: 8px 12px; font-size: 14px; border-radius: 8px; } "
    "QToolBar QToolBarExtension { padding: 0; } "
    "QToolBar QToolButton:checked { background: palette(highlight); color: palette(highlighted-text); }"
);

const float buttonSizeRatio = 1.618f; // golden ratio
constexpr auto settingsDialogDefaultWidth = 950;
constexpr auto settingsDialogDefaultHeight = 500;
const auto settingsNavigationIconTextSpacing = QLatin1String("  ");

/** display name with two lines that is displayed in the settings
 * If width is bigger than 0, the string will be ellided so it does not exceed that width
 */
QString shortDisplayNameForSettings(OCC::Account *account, int width)
{
    QString user = account->prettyName();
    QString host = account->url().host();
    int port = account->url().port();
    if (port > 0 && port != 80 && port != 443) {
        host.append(QLatin1Char(':'));
        host.append(QString::number(port));
    }
    if (width > 0) {
        QFont f;
        QFontMetrics fm(f);
        host = fm.elidedText(host, Qt::ElideMiddle, width);
        user = fm.elidedText(user, Qt::ElideRight, width);
    }
    return QStringLiteral("%1\n%2").arg(user, host);
}
}


namespace OCC {

SettingsDialog::SettingsDialog(ownCloudGui *gui, QWidget *parent)
    : QDialog(parent)
    , _gui(gui)
{
    ConfigFile cfg;

    setupUi();

    // People perceive this as a Window, so also make Ctrl+W work
    auto *closeWindowAction = new QAction(this);
    closeWindowAction->setShortcut(QKeySequence("Ctrl+W"));
    connect(closeWindowAction, &QAction::triggered, this, &SettingsDialog::close);
    addAction(closeWindowAction);

    setObjectName("Settings"); // required as group for saveGeometry call

    //: This name refers to the application name e.g Nextcloud
    setWindowTitle(tr("%1 Settings").arg(Theme::instance()->appNameGUI()));

    connect(AccountManager::instance(), &AccountManager::accountAdded,
        this, &SettingsDialog::accountAdded);
    connect(AccountManager::instance(), &AccountManager::accountRemoved,
        this, &SettingsDialog::accountRemoved);


    _actionGroup = new QActionGroup(this);
    _actionGroup->setExclusive(true);
    connect(_actionGroup, &QActionGroup::triggered, this, &SettingsDialog::slotSwitchPage);

    _stack->setStyleSheet(QStringLiteral("QStackedWidget { background: transparent; }"));

    const auto accountsList = AccountManager::instance()->accounts();
    for (const auto &account : accountsList) {
        accountAdded(account.data());
    }

    auto *accountSpacer = new QWidget(this);
    accountSpacer->setFixedHeight(16);
    _firstNonAccountAction = _toolBar->addWidget(accountSpacer);

    addSettingsPage(QLatin1String(":/client/theme/settings.svg"), tr("General"), new GeneralSettings(this));
    addSettingsPage(QLatin1String(":/client/theme/advanced.svg"), tr("Advanced"), new AdvancedSettings(this));
    addSettingsPage(QLatin1String(":/client/theme/info.svg"), tr("Info"), new InfoSettings(this), true);

    QTimer::singleShot(1, this, &SettingsDialog::showFirstPage);

    auto *showLogWindow = new QAction(this);
    showLogWindow->setShortcut(QKeySequence("F12"));
    connect(showLogWindow, &QAction::triggered, gui, &ownCloudGui::slotToggleLogBrowser);
    addAction(showLogWindow);

    auto *showLogWindow2 = new QAction(this);
    showLogWindow2->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_L));
    connect(showLogWindow2, &QAction::triggered, gui, &ownCloudGui::slotToggleLogBrowser);
    addAction(showLogWindow2);

    connect(this, &SettingsDialog::onActivate, gui, &ownCloudGui::slotSettingsDialogActivated);

    customizeStyle();

    // Close + minimize, but no zoom/full-screen button (full screen makes no sense for Settings).
    setWindowFlags(Qt::Window
        | Qt::CustomizeWindowHint
        | Qt::WindowTitleHint
        | Qt::WindowSystemMenuHint
        | Qt::WindowMinimizeButtonHint
        | Qt::WindowCloseButtonHint);

    // Open centered on screen by default; restoreGeometry() overrides this when a position was saved.
    adjustSize();
    if (const auto *const targetScreen = QGuiApplication::primaryScreen()) {
        move(targetScreen->availableGeometry().center() - rect().center());
    }
    cfg.restoreGeometry(this);
}

SettingsDialog::~SettingsDialog()
{
}

QWidget* SettingsDialog::currentPage()
{
    return _stack->currentWidget();
}

void SettingsDialog::reject()
{
    ConfigFile cfg;
    cfg.saveGeometry(this);
    QDialog::reject();
}

void SettingsDialog::accept()
{
    ConfigFile cfg;
    cfg.saveGeometry(this);
    QDialog::accept();
}

void SettingsDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);

#ifdef Q_OS_MACOS
    // Style the title bar once the native window actually exists. Doing it earlier (e.g. from the
    // constructor via winId()) doesn't stick, because Qt recreates the NSWindow when the dialog is
    // shown, resetting the title-bar separator to its default.
    if (auto *const handle = windowHandle()) {
        styleNativeTitleBar(handle, /*hideTitleText=*/false);
    }
#endif
}

void SettingsDialog::changeEvent(QEvent *e)
{
    switch (e->type()) {
    case QEvent::StyleChange:
    case QEvent::PaletteChange:
    case QEvent::ThemeChange:
        customizeStyle();

        // Notify the other widgets (Dark-/Light-Mode switching)
        emit styleChanged();
#ifdef Q_OS_MACOS
        // macOS resets title-bar styling across appearance changes; re-apply it.
        if (auto *const handle = windowHandle()) {
            styleNativeTitleBar(handle, /*hideTitleText=*/false);
        }
#endif
        break;
    case QEvent::ActivationChange:
        if(isActiveWindow())
            emit onActivate();
        break;
    default:
        break;
    }

    QDialog::changeEvent(e);
}

void SettingsDialog::slotSwitchPage(QAction *action)
{
    _stack->setCurrentWidget(_actionGroupWidgets.value(action));
    _stack->updateGeometry();
    if (auto *contentContainer = _stack->parentWidget()) {
        contentContainer->updateGeometry();
    }
}

void SettingsDialog::showFirstPage()
{
    if (_initialAccount) {
        showAccount(_initialAccount);
        _initialAccount = nullptr;
        return;
    }
    const QList<QAction *> actions = _toolBar->actions();
    for (auto *action : actions) {
        if (_actionGroupWidgets.contains(action)) {
            action->trigger();
            return;
        }
    }
}

void SettingsDialog::setInitialAccount(AccountState *account)
{
    _initialAccount = account;
}

void SettingsDialog::showAccount(AccountState *account)
{
    auto *action = _actionForAccount.value(account->account().data());
    if (action) {
        action->trigger();
    }
}

void SettingsDialog::showIssuesList(AccountState *account)
{
    const auto userModel = UserModel::instance();
    const auto id = userModel->findUserIdForAccount(account);
    UserModel::instance()->setCurrentUserId(id);
    Systray::instance()->showWindow();
}

void SettingsDialog::accountAdded(AccountState *s)
{
    auto height = _toolBar->sizeHint().height();
    bool brandingSingleAccount = !Theme::instance()->multiAccount();

    const auto actionText = brandingSingleAccount ? tr("Account") : s->account()->displayName();
    const auto accountAction = createColorAwareAction(QLatin1String(":/client/theme/account.svg"), actionText);
    updateAccountAvatar(s->account().data());
    
    if (!brandingSingleAccount) {
        accountAction->setToolTip(s->account()->displayName());
        accountAction->setIconText(shortDisplayNameForSettings(s->account().data(), static_cast<int>(height * buttonSizeRatio)));
    }

    if (_firstNonAccountAction) {
        _toolBar->insertAction(_firstNonAccountAction, accountAction);
    } else {
        _toolBar->addAction(accountAction);
    }
    auto accountSettings = new AccountSettings(s, this);
    QString objectName = QLatin1String("accountSettings_");
    objectName += s->account()->displayName();
    accountSettings->setObjectName(objectName);
    _stack->insertWidget(0 , accountSettings);

    _actionGroup->addAction(accountAction);
    _actionGroupWidgets.insert(accountAction, accountSettings);
    _actionForAccount.insert(s->account().data(), accountAction);
    accountAction->trigger();

    connect(accountSettings, &AccountSettings::folderChanged, _gui, &ownCloudGui::slotComputeOverallSyncStatus);
    connect(accountSettings, &AccountSettings::openFolderAlias,
        _gui, &ownCloudGui::slotFolderOpenAction);
    connect(accountSettings, &AccountSettings::showIssuesList, this, &SettingsDialog::showIssuesList);
    connect(s->account().data(), &Account::accountChangedAvatar, this, &SettingsDialog::slotAccountAvatarChanged);
    connect(s->account().data(), &Account::accountChangedDisplayName, this, &SettingsDialog::slotAccountDisplayNameChanged);

    // Connect styleChanged event, to adapt (Dark-/Light-Mode switching)
    connect(this, &SettingsDialog::styleChanged, accountSettings, &AccountSettings::slotStyleChanged);

    const auto userInfo = new UserInfo(s, false, true, this);
    connect(userInfo, &UserInfo::fetchedLastInfo, this, [userInfo](const UserInfo *fetchedInfo) {
        // UserInfo will go and update the account avatar
        Q_UNUSED(fetchedInfo);
        userInfo->deleteLater();
    });
    userInfo->setActive(true);
    userInfo->slotFetchInfo();
}

void SettingsDialog::slotAccountAvatarChanged()
{
    auto *account = dynamic_cast<Account *>(sender());
    if (!account) {
        return;
    }
    updateAccountAvatar(account);
}

void SettingsDialog::updateAccountAvatar(const Account *account)
{
    if (!account || !_actionForAccount.contains(account)) {
        return;
    }

    QAction *action = _actionForAccount[account];
    if (!action) {
        return;
    }

    const QImage pix = account->avatar();
    if (!pix.isNull()) {
        action->setIcon(QPixmap::fromImage(AvatarJob::makeCircularAvatar(pix)));
    }
}

void SettingsDialog::slotAccountDisplayNameChanged()
{
    auto *account = dynamic_cast<Account *>(sender());
    if (account && _actionForAccount.contains(account)) {
        QAction *action = _actionForAccount[account];
        if (action) {
            QString displayName = account->displayName();
            action->setText(displayName);
            auto height = _toolBar->sizeHint().height();
            action->setIconText(shortDisplayNameForSettings(account, static_cast<int>(height * buttonSizeRatio)));
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
            _toolBar->removeAction(it.key());

            if (_stack->currentWidget() == it.value()) {
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

    // Hide when the last account is deleted. We want to enter the same
    // state we'd be in the client was started up without an account
    // configured.
    if (AccountManager::instance()->accounts().isEmpty()) {
        hide();
    }
}

void SettingsDialog::addSettingsPage(const QString &iconPath, const QString &title, QWidget *settingsPage, [[maybe_unused]] bool updateChannelAware)
{
    auto *settingsAction = createColorAwareAction(iconPath, title);
    _actionGroup->addAction(settingsAction);
    _toolBar->addAction(settingsAction);

    QString objectName = QLatin1String("settingsPage_");
    objectName += title;
    settingsPage->setObjectName(objectName);
    _stack->addWidget(settingsPage);

    if (auto *generalSettingsPage = qobject_cast<GeneralSettings *>(settingsPage)) {
        connect(this, &SettingsDialog::styleChanged, generalSettingsPage, &GeneralSettings::slotStyleChanged);
    } else if (auto *advancedSettingsPage = qobject_cast<AdvancedSettings *>(settingsPage)) {
        connect(this, &SettingsDialog::styleChanged, advancedSettingsPage, &AdvancedSettings::slotStyleChanged);
    } else if (auto *infoSettingsPage = qobject_cast<InfoSettings *>(settingsPage)) {
        connect(this, &SettingsDialog::styleChanged, infoSettingsPage, &InfoSettings::slotStyleChanged);

#if defined(BUILD_UPDATER)
        if (updateChannelAware) {
            connect(AccountManager::instance(), &AccountManager::accountAdded, infoSettingsPage, &InfoSettings::loadUpdateChannelsList);
            connect(AccountManager::instance(), &AccountManager::accountRemoved, infoSettingsPage, &InfoSettings::loadUpdateChannelsList);
            connect(AccountManager::instance(), &AccountManager::capabilitiesChanged, infoSettingsPage, &InfoSettings::loadUpdateChannelsList);
        }
#endif
    }

    _actionGroupWidgets.insert(settingsAction, settingsPage);
}

void SettingsDialog::customizeStyle()
{
    if (_updatingStyle) {
        return;
    }

    const QScopedValueRollback<bool> updatingStyle(_updatingStyle, true);
    _toolBar->setStyleSheet(TOOLBAR_CSS);

    auto separatorColor = palette().color(QPalette::Mid);
    separatorColor.setAlpha(48);
    const auto separatorCss = QStringLiteral("rgba(%1, %2, %3, %4)")
        .arg(separatorColor.red())
        .arg(separatorColor.green())
        .arg(separatorColor.blue())
        .arg(separatorColor.alpha());

    setStyleSheet(QStringLiteral(
        "#Settings { background: palette(window); border-radius: 0; }"

        /* Navigation */
        "#settings_navigation_scroll { background: palette(" BACKGROUND_PALETTE "); border-radius: 12px; padding: 4px; }"
        "#settings_navigation { background: transparent; border: none; padding: 0px; }"

        /* Content area */
        "#settings_content, #settings_content_scroll { background: palette(window); border-radius: 12px; }"

        /* Panels */
        "#generalGroupBox, #notificationsGroupBox, #advancedGroupBox, #syncBehaviorGroupBox,"
        "#advancedActionsGroupBox, #aboutAndUpdatesGroupBox, #updatesGroupBox {"
        " background: palette(" BACKGROUND_PALETTE ");"
        " border: none;"
        " border-radius: 12px;"
        " margin: 0px;"
        " padding: 0px;"
        " }"
        "#accountStatusPanel, #encryptionPanel, #fileProviderPanel, #syncFoldersPanel, #accountActionsPanel {"
        " background: palette(" BACKGROUND_PALETTE ");"
        " border: none;"
        " border-radius: 12px;"
        " margin: 0px;"
        " padding: 6px;"
        " }"
        "#generalGroupBox QLabel, #notificationsGroupBox QLabel, #advancedGroupBox QLabel,"
        "#syncBehaviorGroupBox QLabel, #advancedActionsGroupBox QLabel,"
        "#aboutAndUpdatesGroupBox QLabel, #updatesGroupBox QLabel {"
        " margin: 0px;"
        " padding: 0px;"
        " }"
        "#advancedGroupBox QSpinBox, #updatesGroupBox QComboBox {"
        " min-height: 18px;"
        " max-height: 20px;"
        " }"
        "#startupSeparator, #serverNotificationsSeparator, #chatNotificationsSeparator,"
        "#callNotificationsSeparator, #existingFolderLimitSeparator,"
        "#stopExistingFolderNowBigSyncSeparator, #remotePollIntervalSeparator,"
        "#moveFilesToTrashSeparator, #showInExplorerNavigationPaneSeparator,"
        "#updateControlsSeparator {"
        " color: %1;"
        " background: %1;"
        " border: none;"
        " min-height: 1px;"
        " max-height: 1px;"
        " }"
    ).arg(separatorCss));

    const auto &allActions = _actionGroup->actions();
    for (const auto a : allActions) {
        QIcon icon = Theme::createColorAwareIcon(a->property("iconPath").toString(), palette());
        a->setIcon(icon);
        auto *btn = qobject_cast<QToolButton *>(_toolBar->widgetForAction(a));
        if (btn)
            btn->setIcon(icon);
    }
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
            // this means we are in the extension menu, no special action here
            return nullptr;
        }

        class SettingsNavigationButton : public QToolButton
        {
        public:
            using QToolButton::QToolButton;

        protected:
            void paintEvent(QPaintEvent *event) override
            {
                Q_UNUSED(event)

                QStyleOptionToolButton option;
                initStyleOption(&option);

                QPainter painter(this);
                style()->drawComplexControl(QStyle::CC_ToolButton, &option, &painter, this);
            }

        private:
            void initStyleOption(QStyleOptionToolButton *option) const override
            {
                QToolButton::initStyleOption(option);
                if (!option->text.isEmpty()) {
                    option->text.prepend(settingsNavigationIconTextSpacing);
                    option->text.replace(QLatin1Char('\n'), QLatin1Char('\n') + settingsNavigationIconTextSpacing);
                }
            }
        };

        auto *btn = new SettingsNavigationButton(parent);
        QString objectName = QLatin1String("settingsdialog_toolbutton_");
        objectName += text();
        btn->setObjectName(objectName);

        btn->setDefaultAction(this);
        btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        btn->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
        return btn;
    }
};

QAction *SettingsDialog::createActionWithIcon(const QIcon &icon, const QString &text, const QString &iconPath)
{
    QAction *action = new ToolButtonAction(icon, text, this);
    action->setCheckable(true);
    if (!iconPath.isEmpty()) {
        action->setProperty("iconPath", iconPath);
    }
    return action;
}

QAction *SettingsDialog::createColorAwareAction(const QString &iconPath, const QString &text)
{
    // all buttons must have the same size in order to keep a good layout
    QIcon coloredIcon = Theme::createColorAwareIcon(iconPath, palette());
    return createActionWithIcon(coloredIcon, text, iconPath);
}

void SettingsDialog::setupUi()
{
    setWindowTitle(tr("Settings"));
    setGeometry(0, 0, settingsDialogDefaultWidth, settingsDialogDefaultHeight);
    setMinimumSize(settingsDialogDefaultWidth, settingsDialogDefaultHeight);

    auto *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(12);
    setLayout(mainLayout);

    _toolBar = new QToolBar;
    _toolBar->setIconSize(QSize(32, 32));
    _toolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    _toolBar->setOrientation(Qt::Vertical);
    _toolBar->setMovable(false);
    _toolBar->setMinimumWidth(220);
    _toolBar->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);

    auto *navigationContainer = new QWidget(this);
    navigationContainer->setObjectName("settings_navigation"_L1);
    navigationContainer->setAttribute(Qt::WA_StyledBackground);

    auto *navigationLayout = new QVBoxLayout(navigationContainer);
    navigationLayout->setContentsMargins(0, 0, 0, 0);
    navigationLayout->setSpacing(0);
    navigationLayout->addWidget(_toolBar);
    navigationLayout->addStretch(1);

    auto *navigationScroll = new QScrollArea(this);
    navigationScroll->setObjectName("settings_navigation_scroll"_L1);
    navigationScroll->setWidgetResizable(true);
    navigationScroll->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    navigationScroll->setFrameShape(QFrame::NoFrame);
    navigationScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    navigationScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    navigationScroll->viewport()->setAutoFillBackground(false);
    navigationScroll->setWidget(navigationContainer);

    _stack = new CurrentPageSizeStackedWidget(this);
    _stack->setObjectName(u"settings_content"_s);

    auto *contentScroll = new QScrollArea(this);
    contentScroll->setObjectName("settings_content_scroll"_L1);
    contentScroll->setWidgetResizable(true);
    contentScroll->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    contentScroll->setFrameShape(QFrame::NoFrame);
    contentScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    contentScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    contentScroll->viewport()->setAutoFillBackground(false);
    contentScroll->setWidget(_stack);

    mainLayout->addWidget(navigationScroll);
    mainLayout->addWidget(contentScroll);
    mainLayout->setStretch(0, 0);
    mainLayout->setStretch(1, 1);
}

} // namespace OCC
