/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2013 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "settingsdialog.h"

#include "folderman.h"
#include "theme.h"
#include "generalsettings.h"
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
#include <QQuickView>
#include <QActionGroup>
#include <QScopedValueRollback>
#include <QScrollArea>
#include <QSizePolicy>
#include <QTimer>
#include <QMouseEvent>
#include <QWindow>
#include <QtGlobal>

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

class WindowDragHandle : public QWidget
{
public:
    using QWidget::QWidget;

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton) {
            if (const auto *window = this->window(); window && window->windowHandle()) {
                window->windowHandle()->startSystemMove();
                event->accept();
                return;
            }
        }

        QWidget::mousePressEvent(event);
    }
};

SettingsDialog::SettingsDialog(ownCloudGui *gui, QWidget *parent)
    : QDialog(parent)
    , _gui(gui)
{
#if defined(Q_OS_MACOS) && QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    setWindowFlag(Qt::ExpandedClientAreaHint, true);
    setWindowFlag(Qt::NoTitleBarBackgroundHint, true);
#endif

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

    QAction *generalAction = createColorAwareAction(QLatin1String(":/client/theme/settings.svg"), tr("General"));
    _actionGroup->addAction(generalAction);
    _toolBar->addAction(generalAction);
    auto *accountSpacer = new QWidget(this);
    accountSpacer->setFixedHeight(16);
    _toolBar->addWidget(accountSpacer);
    _toolBar->addSeparator();
    auto *generalSettings = new GeneralSettings;
    _stack->addWidget(generalSettings);
    _stack->setStyleSheet(QStringLiteral("QStackedWidget { background: transparent; }"));

    // Connect styleChanged events to our widgets, so they can adapt (Dark-/Light-Mode switching)
    connect(this, &SettingsDialog::styleChanged, generalSettings, &GeneralSettings::slotStyleChanged);

#if defined(BUILD_UPDATER)
    connect(AccountManager::instance(), &AccountManager::accountAdded, generalSettings, &GeneralSettings::loadUpdateChannelsList);
    connect(AccountManager::instance(), &AccountManager::accountRemoved, generalSettings, &GeneralSettings::loadUpdateChannelsList);
    connect(AccountManager::instance(), &AccountManager::capabilitiesChanged, generalSettings, &GeneralSettings::loadUpdateChannelsList);
#endif

    _actionGroupWidgets.insert(generalAction, generalSettings);

    const auto accountsList = AccountManager::instance()->accounts();
    for (const auto &account : accountsList) {
        accountAdded(account.data());
    }

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

    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    setWindowFlag(Qt::Window, true);
    cfg.restoreGeometry(this);
}

SettingsDialog::~SettingsDialog()
{
}

QWidget* SettingsDialog::currentPage()
{
    return _stack->currentWidget();
}

// close event is not being called here
void SettingsDialog::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);

#if defined(Q_OS_MACOS) && QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    if (_windowDragHandle) {
        _windowDragHandle->setGeometry(0, 0, width(), _windowDragHandle->height());
        _windowDragHandle->raise();
    }
#endif
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

void SettingsDialog::changeEvent(QEvent *e)
{
    switch (e->type()) {
    case QEvent::StyleChange:
    case QEvent::PaletteChange:
    case QEvent::ThemeChange:
        customizeStyle();

        // Notify the other widgets (Dark-/Light-Mode switching)
        emit styleChanged();
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
    QList<QAction *> actions = _toolBar->actions();
    if (!actions.empty()) {
        actions.first()->trigger();
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

    _toolBar->addAction(accountAction);
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

void SettingsDialog::customizeStyle()
{
    if (_updatingStyle) {
        return;
    }

    const QScopedValueRollback<bool> updatingStyle(_updatingStyle, true);
    _toolBar->setStyleSheet(TOOLBAR_CSS);

    setStyleSheet(QStringLiteral(
        "#Settings { background: palette(window); border-radius: 0; }"

        /* Navigation */
        "#settings_navigation, #settings_navigation_scroll { background: palette(" BACKGROUND_PALETTE "); border-radius: 12px; padding: 4px; }"

        /* Content area */
        "#settings_content, #settings_content_scroll { background: palette(window); border-radius: 12px; }"

        /* Panels */
        "#generalGroupBox, #advancedGroupBox, #aboutAndUpdatesGroupBox,"
        "#accountStatusPanel, #connectionSettingsPanel, #fileProviderPanel, #syncFoldersPanel {"
        " background: palette(" BACKGROUND_PALETTE ");"
        " border-radius: 10px;"
        " margin: 0px;"
        " padding: 6px;"
        " }"
        "#generalGroupBoxTitle, #advancedGroupBoxTitle, #aboutAndUpdatesGroupBoxTitle {"
        " margin-bottom: 6px;"
        " }"
    ));

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

        auto *btn = new QToolButton(parent);
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
    setGeometry(0, 0, 950, 500);

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

#if defined(Q_OS_MACOS) && QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    _windowDragHandle = new WindowDragHandle(this);
    _windowDragHandle->setObjectName(QLatin1String("settings_window_drag_handle"));
    _windowDragHandle->setFixedHeight(28);
    _windowDragHandle->setGeometry(0, 0, width(), _windowDragHandle->height());
    _windowDragHandle->raise();
#endif
}

} // namespace OCC
