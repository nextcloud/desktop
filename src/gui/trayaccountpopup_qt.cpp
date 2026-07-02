/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "systray.h"

#include "accountmanager.h"
#include "accountstate.h"
#include "iconjob.h"
#include "theme.h"
#include "tray/trayaccountappsmodel.h"
#include "tray/trayimageutils.h"
#include "tray/usermodel.h"

#include <QAction>
#include <QAbstractItemModel>
#include <QColor>
#include <QCoreApplication>
#include <QCursor>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QHash>
#include <QIcon>
#include <QImage>
#include <QList>
#include <QMenu>
#include <QMimeDatabase>
#include <QMimeType>
#include <QModelIndex>
#include <QPalette>
#include <QPainter>
#include <QPointer>
#include <QPixmap>
#include <QScreen>
#include <QSize>
#include <QStyle>
#include <QSvgRenderer>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>

#include <algorithm>

namespace OCC {

// Keep behavior and menu taxonomy aligned with src/gui/macOS/trayaccountpopup_mac.mm.

namespace {

constexpr auto fixedMenuWidth = 320;
// QMenu already reserves the icon/check column when painting; only reserve the
// remaining native chrome and submenu indicator for manual eliding.
constexpr auto menuTextReservedWidth = 40;
constexpr auto submenuIndicatorReservedWidth = 18;
constexpr auto minimumElidedTextWidth = 32;

QPointer<QMenu> s_trayPopup;
QHash<QString, QIcon> s_remoteAppIconCache;

void setFixedMenuWidth(QMenu *menu)
{
    if (menu) {
        menu->setFixedWidth(fixedMenuWidth);
        menu->setToolTipsVisible(true);
    }
}

QSize nativeMenuIconSize(const QMenu *menu)
{
    static constexpr auto fallbackIconSize = 16;

    auto iconSize = fallbackIconSize;
    if (menu && menu->style()) {
        const auto styleIconSize = menu->style()->pixelMetric(QStyle::PM_SmallIconSize, nullptr, menu);
        if (styleIconSize > 0) {
            iconSize = styleIconSize;
        }
    }
    return QSize(iconSize, iconSize);
}

bool isWaylandPlatform()
{
    return QGuiApplication::platformName().contains(QStringLiteral("wayland"), Qt::CaseInsensitive);
}

QImage imageFromImageData(const QByteArray &imageData, const QSize &requestedSize)
{
    if (imageData.isEmpty()) {
        return {};
    }

    const auto mimetype = QMimeDatabase().mimeTypeForData(imageData);
    if (mimetype.isValid() && mimetype.inherits(QStringLiteral("image/svg+xml"))) {
        auto renderer = QSvgRenderer{};
        if (!renderer.load(imageData)) {
            return {};
        }

        auto image = QImage(requestedSize, QImage::Format_ARGB32);
        image.fill(Qt::transparent);
        auto painter = QPainter(&image);
        renderer.render(&painter, aspectFitRect(renderer.defaultSize(), requestedSize));
        return image;
    }

    auto image = QImage::fromData(imageData);
    if (!image.isNull() && requestedSize.isValid()) {
        image = image.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    return image;
}

QImage imageFromSvgIcon(const QString &iconPath, const QSize &requestedSize)
{
    auto renderer = QSvgRenderer{iconPath};
    if (!renderer.isValid()) {
        return {};
    }

    auto image = QImage(requestedSize, QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    auto painter = QPainter(&image);
    renderer.render(&painter, aspectFitRect(renderer.defaultSize(), requestedSize));
    return image;
}

QImage tintImage(const QImage &image, const QColor &color)
{
    if (image.isNull() || !color.isValid()) {
        return image;
    }

    auto tintedImage = QImage(image.size(), QImage::Format_ARGB32_Premultiplied);
    tintedImage.fill(color);

    auto painter = QPainter(&tintedImage);
    painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
    painter.drawImage(0, 0, image.convertToFormat(QImage::Format_ARGB32_Premultiplied));
    painter.end();
    return tintedImage;
}

QPalette nativeMenuIconPalette(const QMenu *menu)
{
    if (menu && menu->style()) {
        return menu->style()->standardPalette();
    }
    return QGuiApplication::palette();
}

QString templateIconPaletteCacheKey(const QPalette &palette)
{
    return QStringLiteral("%1:%2:%3").arg(
        palette.color(QPalette::Active, QPalette::Text).name(QColor::HexArgb),
        palette.color(QPalette::Active, QPalette::HighlightedText).name(QColor::HexArgb),
        palette.color(QPalette::Disabled, QPalette::Text).name(QColor::HexArgb));
}

void addTemplatePixmap(QIcon &icon, const QImage &image, const QColor &color, const QIcon::Mode mode)
{
    icon.addPixmap(QPixmap::fromImage(tintImage(image, color)), mode);
}

QIcon templateIconFromImage(const QImage &image, const QPalette &palette)
{
    if (image.isNull()) {
        return {};
    }

    auto icon = QIcon{};
    addTemplatePixmap(icon, image, palette.color(QPalette::Active, QPalette::Text), QIcon::Normal);
    addTemplatePixmap(icon, image, palette.color(QPalette::Active, QPalette::Text), QIcon::Active);
    addTemplatePixmap(icon, image, palette.color(QPalette::Active, QPalette::HighlightedText), QIcon::Selected);
    addTemplatePixmap(icon, image, palette.color(QPalette::Disabled, QPalette::Text), QIcon::Disabled);
    return icon;
}

QIcon templateIconFromIcon(const QIcon &icon, const QSize &requestedSize, const QPalette &palette)
{
    if (icon.isNull()) {
        return {};
    }

    return templateIconFromImage(icon.pixmap(requestedSize).toImage(), palette);
}

QIcon templateIconFromSvg(const QString &iconPath, const QSize &requestedSize, const QPalette &palette)
{
    return templateIconFromImage(imageFromSvgIcon(iconPath, requestedSize), palette);
}

QIcon templateThemeIcon(const QString &iconName, const QSize &requestedSize, const QPalette &palette)
{
    return templateIconFromSvg(QStringLiteral(":/client/theme/%1").arg(iconName), requestedSize, palette);
}

QIcon templateBlackThemeIcon(const QString &iconName, const QSize &requestedSize, const QPalette &palette)
{
    return templateIconFromSvg(QStringLiteral(":/client/theme/black/%1").arg(iconName), requestedSize, palette);
}

QString statusText(const UserStatus::OnlineStatus status)
{
    switch (status) {
    case UserStatus::OnlineStatus::Online:
        return QCoreApplication::translate("UserStatusSetStatusView", "Online");
    case UserStatus::OnlineStatus::Away:
        return QCoreApplication::translate("UserStatusSetStatusView", "Away");
    case UserStatus::OnlineStatus::Busy:
        return QCoreApplication::translate("UserStatusSetStatusView", "Busy");
    case UserStatus::OnlineStatus::DoNotDisturb:
        return QCoreApplication::translate("UserStatusSetStatusView", "Do not disturb");
    case UserStatus::OnlineStatus::Invisible:
        return QCoreApplication::translate("UserStatusSetStatusView", "Invisible");
    case UserStatus::OnlineStatus::Offline:
        return QCoreApplication::translate("OCC::SyncStatusSummary", "Offline");
    }
    return QCoreApplication::translate("UserStatusSetStatusView", "Online");
}

QString statusMenuText(const UserStatus::OnlineStatus status, const QString &message)
{
    const auto trimmedMessage = message.trimmed();
    return trimmedMessage.isEmpty() ? statusText(status) : trimmedMessage;
}

QIcon themeIcon(const QString &iconName)
{
    return Theme::createColorAwareIcon(QStringLiteral(":/client/theme/%1").arg(iconName));
}

QIcon blackThemeIcon(const QString &iconName)
{
    return Theme::createColorAwareIcon(QStringLiteral(":/client/theme/black/%1").arg(iconName));
}

QIcon iconFromUrl(const QUrl &url)
{
    if (url.isEmpty()) {
        return {};
    }
    if (url.isLocalFile()) {
        return QIcon(url.toLocalFile());
    }
    if (url.scheme() == QStringLiteral("qrc")) {
        return QIcon(QStringLiteral(":%1").arg(url.path()));
    }
    if (url.scheme().isEmpty()) {
        return QIcon(url.toString());
    }
    return {};
}

QIcon iconFromImage(const QImage &image)
{
    return image.isNull() ? QIcon{} : QIcon(QPixmap::fromImage(image));
}

bool isRemoteIconUrl(const QUrl &url)
{
    return url.scheme() == QStringLiteral("http") || url.scheme() == QStringLiteral("https");
}

QString remoteIconCacheKey(const AccountStatePtr &accountState,
    const QUrl &url,
    const QSize &requestedSize,
    const QPalette &iconPalette)
{
    if (!accountState || !accountState->account()) {
        return {};
    }
    return QStringLiteral("%1:%2x%3:%4:%5").arg(
        accountState->account()->id(),
        QString::number(requestedSize.width()),
        QString::number(requestedSize.height()),
        templateIconPaletteCacheKey(iconPalette),
        url.toString());
}

void fetchRemoteAppIcon(QAction *action,
    const AccountStatePtr &accountState,
    const QUrl &iconUrl,
    const QSize &requestedSize,
    const QPalette &iconPalette)
{
    if (!action || !accountState || !accountState->account() || !isRemoteIconUrl(iconUrl)) {
        return;
    }

    const auto cacheKey = remoteIconCacheKey(accountState, iconUrl, requestedSize, iconPalette);
    if (cacheKey.isEmpty()) {
        return;
    }

    if (s_remoteAppIconCache.contains(cacheKey)) {
        action->setIcon(s_remoteAppIconCache.value(cacheKey));
        return;
    }

    const auto actionPointer = QPointer<QAction>(action);
    auto iconJob = new IconJob(accountState->account(), iconUrl, action);
    QObject::connect(iconJob,
        &IconJob::jobFinished,
        action,
        [actionPointer, cacheKey, requestedSize, iconPalette](const QByteArray &iconData) {
            const auto image = imageFromImageData(iconData, requestedSize);
            if (image.isNull()) {
                return;
            }

            const auto icon = templateIconFromImage(image, iconPalette);
            s_remoteAppIconCache.insert(cacheKey, icon);
            if (actionPointer) {
                actionPointer->setIcon(icon);
            }
        });
}

QIcon activityIcon(const QVariantMap &activityData)
{
    const auto systemIconName = activityData.value(QStringLiteral("systemIconName")).toString();
    if (systemIconName == QStringLiteral("bell")) {
        return blackThemeIcon(QStringLiteral("bell.svg"));
    }
    if (systemIconName == QStringLiteral("exclamationmark.triangle")) {
        return themeIcon(QStringLiteral("warning.svg"));
    }
    if (systemIconName == QStringLiteral("trash")) {
        return themeIcon(QStringLiteral("delete.svg"));
    }
    if (systemIconName == QStringLiteral("pencil")) {
        return themeIcon(QStringLiteral("change.svg"));
    }
    if (systemIconName == QStringLiteral("message")) {
        return blackThemeIcon(QStringLiteral("comment.svg"));
    }
    if (systemIconName == QStringLiteral("calendar")) {
        return blackThemeIcon(QStringLiteral("calendar.svg"));
    }
    return blackThemeIcon(QStringLiteral("activity.svg"));
}

QString timedMenuText(const QString &text, const QString &dateTime)
{
    if (!dateTime.isEmpty()) {
        return text + QStringLiteral(" - ") + dateTime;
    }
    return text;
}

int menuTextWidth(const bool hasSubmenu = false)
{
    auto textWidth = fixedMenuWidth - menuTextReservedWidth;
    if (hasSubmenu) {
        textWidth -= submenuIndicatorReservedWidth;
    }

    return std::max(textWidth, minimumElidedTextWidth);
}

QString elidedMenuText(const QMenu *menu, const QString &text, const bool hasSubmenu = false)
{
    if (!menu || text.isEmpty()) {
        return text;
    }

    return menu->fontMetrics().elidedText(text, Qt::ElideRight, menuTextWidth(hasSubmenu));
}

QString elidedTimedMenuText(const QMenu *menu, const QString &text, const QString &dateTime, const bool hasSubmenu = false)
{
    if (!menu || text.isEmpty() || dateTime.isEmpty()) {
        return elidedMenuText(menu, timedMenuText(text, dateTime), hasSubmenu);
    }

    const auto timeSuffix = QStringLiteral(" - ") + dateTime;
    const auto availableTextWidth = menuTextWidth(hasSubmenu);
    const auto messageTextWidth = std::max(
        availableTextWidth - menu->fontMetrics().horizontalAdvance(timeSuffix),
        minimumElidedTextWidth);
    return menu->fontMetrics().elidedText(text, Qt::ElideRight, messageTextWidth) + timeSuffix;
}

void setFullTextTooltip(QAction *action, const QString &fullText, const QString &displayText)
{
    if (action && fullText != displayText) {
        action->setToolTip(fullText);
    }
}

QAction *addMenuAction(QMenu *menu, const QIcon &icon, const QString &text)
{
    const auto displayText = elidedMenuText(menu, text);
    const auto action = icon.isNull() ? menu->addAction(displayText) : menu->addAction(icon, displayText);
    setFullTextTooltip(action, text, displayText);
    return action;
}

QAction *addTimedMenuAction(QMenu *menu, const QIcon &icon, const QString &text, const QString &dateTime)
{
    const auto fullText = timedMenuText(text, dateTime);
    const auto displayText = elidedTimedMenuText(menu, text, dateTime);
    const auto action = icon.isNull() ? menu->addAction(displayText) : menu->addAction(icon, displayText);
    setFullTextTooltip(action, fullText, displayText);
    return action;
}

QMenu *addSubMenu(QMenu *menu, const QIcon &icon, const QString &text)
{
    const auto displayText = elidedMenuText(menu, text, true);
    auto subMenu = icon.isNull() ? menu->addMenu(displayText) : menu->addMenu(icon, displayText);
    if (subMenu) {
        setFullTextTooltip(subMenu->menuAction(), text, displayText);
    }
    return subMenu;
}

QMenu *addTimedSubMenu(QMenu *menu, const QIcon &icon, const QString &text, const QString &dateTime)
{
    const auto fullText = timedMenuText(text, dateTime);
    const auto displayText = elidedTimedMenuText(menu, text, dateTime, true);
    auto subMenu = icon.isNull() ? menu->addMenu(displayText) : menu->addMenu(icon, displayText);
    if (subMenu) {
        setFullTextTooltip(subMenu->menuAction(), fullText, displayText);
    }
    return subMenu;
}

void clearDynamicMenu(QMenu *menu)
{
    if (!menu) {
        return;
    }

    const auto childMenus = menu->findChildren<QMenu *>(QString{}, Qt::FindDirectChildrenOnly);
    menu->clear();
    for (auto childMenu : childMenus) {
        delete childMenu;
    }
}

QAction *addSectionHeading(QMenu *menu, const QString &text)
{
    const auto displayText = elidedMenuText(menu, text);
    const auto action = menu->addAction(displayText);
    setFullTextTooltip(action, text, displayText);
    action->setEnabled(false);
    action->setIconVisibleInMenu(false);
    return action;
}

QPoint clampedMenuPosition(const QPoint &position, const QSize &menuSize, const QRect &availableGeometry)
{
    auto clampedPosition = position;
    if (clampedPosition.x() + menuSize.width() > availableGeometry.right() + 1) {
        clampedPosition.setX(availableGeometry.right() - menuSize.width() + 1);
    }
    if (clampedPosition.x() < availableGeometry.left()) {
        clampedPosition.setX(availableGeometry.left());
    }
    if (clampedPosition.y() + menuSize.height() > availableGeometry.bottom() + 1) {
        clampedPosition.setY(availableGeometry.bottom() - menuSize.height() + 1);
    }
    if (clampedPosition.y() < availableGeometry.top()) {
        clampedPosition.setY(availableGeometry.top());
    }
    return clampedPosition;
}

void closeTrayPopup()
{
    if (s_trayPopup) {
        s_trayPopup->close();
    }
}

void openActivitiesForUser(const int userId)
{
    closeTrayPopup();
    Systray::instance()->showActivitiesWindow(userId);
}

void openLocalFolderForUser(const int userId)
{
    closeTrayPopup();

    const auto userModel = UserModel::instance();
    if (!userModel) {
        return;
    }

    userModel->setCurrentUserId(userId);
    const auto user = userModel->currentUser();
    if (!user) {
        return;
    }

    if (user->hasLocalFolder()) {
        userModel->openCurrentAccountLocalFolder();
    }
#ifdef BUILD_FILE_PROVIDER_MODULE
    else if (user->hasFileProvider()) {
        userModel->openCurrentAccountFileProviderDomain();
    }
#endif
}

void openUserStatusForUser(const int userId)
{
    closeTrayPopup();
    Systray::instance()->showUserStatusWindow(userId);
}

void openAssistantForUser(const int userId)
{
    closeTrayPopup();
    Systray::instance()->showAssistantWindow(userId);
}

void openSettingsAfterTrayPopupCloses()
{
    closeTrayPopup();
    QTimer::singleShot(0, Systray::instance(), [] {
        Systray::instance()->openSettings();
    });
}

void openNotification(const int userId, const bool opensSettings)
{
    if (opensSettings) {
        openSettingsAfterTrayPopupCloses();
        return;
    }
    closeTrayPopup();
    Systray::instance()->showActivitiesWindow(userId);
}

bool populateAppsMenu(QMenu *menu, const int userId)
{
    setFixedMenuWidth(menu);
    clearDynamicMenu(menu);

    const auto appsModel = TrayAccountAppsModel::instance();
    appsModel->setUserId(userId);
    const auto appCount = appsModel->rowCount();
    const auto accounts = AccountManager::instance()->accounts();
    const auto accountState = userId >= 0 && userId < accounts.size()
        ? accounts.at(userId)
        : AccountStatePtr{};
    const auto appIconPalette = nativeMenuIconPalette(menu);
    const auto appIconSize = nativeMenuIconSize(menu);

    for (auto row = 0; row < appCount; ++row) {
        const auto appIndex = appsModel->index(row);
        const auto appName = appsModel->data(appIndex, TrayAccountAppsModel::NameRole).toString();
        const auto appUrl = appsModel->data(appIndex, TrayAccountAppsModel::UrlRole).toUrl();
        const auto appIconUrl = appsModel->data(appIndex, TrayAccountAppsModel::IconUrlRole).toUrl();
        auto appIcon = iconFromUrl(appIconUrl);
        if (appIcon.isNull()) {
            appIcon = templateBlackThemeIcon(QStringLiteral("more-apps.svg"), appIconSize, appIconPalette);
        } else {
            appIcon = templateIconFromIcon(appIcon, appIconSize, appIconPalette);
        }

        const auto action = addMenuAction(menu, appIcon, appName);
        fetchRemoteAppIcon(action, accountState, appIconUrl, appIconSize, appIconPalette);
        QObject::connect(action, &QAction::triggered, action, [appUrl] {
            closeTrayPopup();
            TrayAccountAppsModel::instance()->openAppUrl(appUrl);
        });
    }

    if (menu->isEmpty()) {
        const auto noAppsAction = addMenuAction(menu, QIcon{}, QCoreApplication::translate("TrayAccountPopup", "No apps available"));
        noAppsAction->setEnabled(false);
    }

    return appCount > 0;
}

void populateNotificationActionsMenu(QMenu *menu, const int userId, const int activityIndex, const QVariantList &actions)
{
    for (const auto &actionVariant : actions) {
        const auto actionData = actionVariant.toMap();
        const auto title = actionData.value(QStringLiteral("label")).toString();
        if (title.isEmpty()) {
            continue;
        }

        const auto actionType = actionData.value(QStringLiteral("actionType")).toString();
        const auto actionIndex = actionData.value(QStringLiteral("actionIndex")).toInt();
        const auto action = addMenuAction(menu, QIcon{}, title);
        QObject::connect(action, &QAction::triggered, action, [userId, activityIndex, actionType, actionIndex] {
            if (actionType == QStringLiteral("dismiss")) {
                UserModel::instance()->dismissNotification(userId, activityIndex);
                return;
            }
            if (actionType == QStringLiteral("openActivities")) {
                openActivitiesForUser(userId);
                return;
            }
            closeTrayPopup();
            UserModel::instance()->triggerNotificationAction(userId, activityIndex, actionIndex);
        });
    }
}

void addNotifications(QMenu *menu, const int userId, const QVariantList &notifications)
{
    if (notifications.isEmpty()) {
        return;
    }

    addSectionHeading(menu, QCoreApplication::translate("TrayAccountPopup", "Notifications"));
    for (const auto &notificationVariant : notifications) {
        const auto notificationData = notificationVariant.toMap();
        const auto title = notificationData.value(QStringLiteral("title")).toString();
        if (title.isEmpty()) {
            continue;
        }

        const auto opensSettings = notificationData.value(QStringLiteral("opensSettings")).toBool();
        const auto activityIndex = notificationData.value(QStringLiteral("activityIndex")).toInt();
        const auto actions = notificationData.value(QStringLiteral("actions")).toList();
        const auto dateTime = notificationData.value(QStringLiteral("dateTime")).toString();
        if (actions.isEmpty()) {
            const auto action = addTimedMenuAction(menu, activityIcon(notificationData), title, dateTime);
            QObject::connect(action, &QAction::triggered, action, [userId, opensSettings] {
                openNotification(userId, opensSettings);
            });
            continue;
        }

        const auto notificationMenu = addTimedSubMenu(menu, activityIcon(notificationData), title, dateTime);
        setFixedMenuWidth(notificationMenu);
        const auto openAction = addMenuAction(notificationMenu, QIcon{}, QCoreApplication::translate("TrayAccountPopup", "Open"));
        QObject::connect(openAction, &QAction::triggered, openAction, [userId, opensSettings] {
            openNotification(userId, opensSettings);
        });
        notificationMenu->addSeparator();
        populateNotificationActionsMenu(notificationMenu, userId, activityIndex, actions);
    }

    menu->addSeparator();
}

void addRecentActivities(QMenu *menu, const int userId, const QVariantList &recentActivities)
{
    addSectionHeading(menu, QCoreApplication::translate("TrayAccountPopup", "Recent activity"));

    if (recentActivities.isEmpty()) {
        const auto menuIconPalette = nativeMenuIconPalette(menu);
        const auto menuIconSize = nativeMenuIconSize(menu);
        const auto noRecentActivity = addMenuAction(menu,
            templateBlackThemeIcon(QStringLiteral("activity.svg"), menuIconSize, menuIconPalette),
            QCoreApplication::translate("TrayAccountPopup", "No recent activity"));
        noRecentActivity->setEnabled(false);
    }

    for (const auto &recentActivityVariant : recentActivities) {
        const auto activityData = recentActivityVariant.toMap();
        const auto message = activityData.value(QStringLiteral("subtitle")).toString();
        if (message.isEmpty()) {
            continue;
        }

        const auto dateTime = activityData.value(QStringLiteral("dateTime")).toString();
        const auto action = addTimedMenuAction(menu, activityIcon(activityData), message, dateTime);
        QObject::connect(action, &QAction::triggered, action, [userId] {
            openActivitiesForUser(userId);
        });
    }

    const auto moreActivitiesAction = addMenuAction(menu, QIcon{}, QCoreApplication::translate("TrayAccountPopup", "More activity\342\200\246"));
    QObject::connect(moreActivitiesAction, &QAction::triggered, moreActivitiesAction, [userId] {
        openActivitiesForUser(userId);
    });
}

void populateAccountMenu(QMenu *menu, const int userId, const bool fetchActivityPreview = true)
{
    setFixedMenuWidth(menu);
    clearDynamicMenu(menu);

    const auto userModel = UserModel::instance();
    if (!userModel || userId < 0 || userId >= userModel->rowCount()) {
        return;
    }

    if (fetchActivityPreview) {
        userModel->fetchActivityPreview(userId);
    }

    const auto userModelIndex = userModel->index(userId);
    const auto menuIconPalette = nativeMenuIconPalette(menu);
    const auto menuIconSize = nativeMenuIconSize(menu);
    const auto serverHasUserStatus = userModel->data(userModelIndex, UserModel::ServerHasUserStatusRole).toBool();
    const auto onlineStatusEnabled = userModel->data(userModelIndex, UserModel::IsConnectedRole).toBool() && serverHasUserStatus;

    const auto accountAlert = userModel->data(userModelIndex, UserModel::AccountAlertRole).toMap();
    const auto accountAlertTitle = accountAlert.value(QStringLiteral("title")).toString();
    if (!accountAlertTitle.isEmpty()) {
        const auto resolveAction = addMenuAction(menu,
            themeIcon(QStringLiteral("warning.svg")),
            QCoreApplication::translate("TrayAccountPopup", "Resolve: %1").arg(accountAlertTitle));
        QObject::connect(resolveAction, &QAction::triggered, resolveAction, [userId] {
            openActivitiesForUser(userId);
        });
    }

    if (serverHasUserStatus) {
        addSectionHeading(menu, QCoreApplication::translate("TrayAccountPopup", "User status"));
        const auto status = userModel->data(userModelIndex, UserModel::StatusRole).value<UserStatus::OnlineStatus>();
        const auto statusMessage = userModel->data(userModelIndex, UserModel::StatusMessageRole).toString();
        const auto statusAction = addMenuAction(menu,
            iconFromUrl(userModel->data(userModelIndex, UserModel::StatusIconRole).toUrl()),
            statusMenuText(status, statusMessage));
        statusAction->setEnabled(onlineStatusEnabled);
        QObject::connect(statusAction, &QAction::triggered, statusAction, [userId] {
            openUserStatusForUser(userId);
        });
    }

    menu->addSeparator();

    const auto openFolderAction = addMenuAction(menu,
        templateBlackThemeIcon(QStringLiteral("folder.svg"), menuIconSize, menuIconPalette),
        QCoreApplication::translate("TrayFoldersMenuButton", "Local folder"));
    QObject::connect(openFolderAction, &QAction::triggered, openFolderAction, [userId] {
        openLocalFolderForUser(userId);
    });

    const auto assistantEnabled = userModel->data(userModelIndex, UserModel::AssistantEnabledRole).toBool();
    if (assistantEnabled) {
        const auto assistantAction = addMenuAction(menu,
            templateBlackThemeIcon(QStringLiteral("nc-assistant-app.svg"), menuIconSize, menuIconPalette),
            QCoreApplication::translate("MainWindow", "Ask Assistant\302\240\342\200\246"));
        QObject::connect(assistantAction, &QAction::triggered, assistantAction, [userId] {
            openAssistantForUser(userId);
        });
    }

    const auto appsMenu = addSubMenu(menu,
        templateBlackThemeIcon(QStringLiteral("more-apps.svg"), menuIconSize, menuIconPalette),
        QCoreApplication::translate("TrayWindowHeader", "More apps"));
    setFixedMenuWidth(appsMenu);
    appsMenu->menuAction()->setEnabled(populateAppsMenu(appsMenu, userId));
    QObject::connect(appsMenu, &QMenu::aboutToShow, appsMenu, [appsMenu, userId] {
        appsMenu->menuAction()->setEnabled(populateAppsMenu(appsMenu, userId));
    });

    menu->addSeparator();

    addNotifications(menu, userId, userModel->data(userModelIndex, UserModel::TrayNotificationsRole).toList());
    addRecentActivities(menu, userId, userModel->data(userModelIndex, UserModel::RecentActivitiesRole).toList());
}

void populateTrayMenu(QMenu *menu, Systray *systray)
{
    if (!systray) {
        return;
    }

    setFixedMenuWidth(menu);
    clearDynamicMenu(menu);

    const auto userModel = UserModel::instance();
    const auto menuIconPalette = nativeMenuIconPalette(menu);
    const auto menuIconSize = nativeMenuIconSize(menu);
    if (userModel && userModel->rowCount() > 0) {
        for (auto userId = 0; userId < userModel->rowCount(); ++userId) {
            const auto userModelIndex = userModel->index(userId);
            const auto name = userModel->data(userModelIndex, UserModel::NameRole).toString();
            const auto server = userModel->data(userModelIndex, UserModel::ServerRole).toString();
            const auto accountText = QStringLiteral("%1 (%2)").arg(name, server);
            auto accountIcon = iconFromImage(userModel->syncStatusIconForRow(userId));
            if (accountIcon.isNull()) {
                accountIcon = Theme::instance()->applicationIcon();
            }

            const auto accountMenu = addSubMenu(menu, accountIcon, accountText);
            setFixedMenuWidth(accountMenu);
            QObject::connect(accountMenu, &QMenu::aboutToShow, accountMenu, [accountMenu, userId] {
                populateAccountMenu(accountMenu, userId);
            });
            QObject::connect(userModel,
                &QAbstractItemModel::dataChanged,
                accountMenu,
                [accountMenu, userId](const QModelIndex &topLeft, const QModelIndex &bottomRight, const QList<int> &roles) {
                    if (!accountMenu->isVisible() || userId < topLeft.row() || userId > bottomRight.row()) {
                        return;
                    }
                    if (!roles.isEmpty()
                        && !roles.contains(UserModel::RecentActivitiesRole)
                        && !roles.contains(UserModel::TrayNotificationsRole)) {
                        return;
                    }
                    populateAccountMenu(accountMenu, userId, false);
                });
        }
        menu->addSeparator();
    }

    if (systray->enableAddAccount()) {
        const auto addAccountAction = addMenuAction(menu,
            templateThemeIcon(QStringLiteral("add.svg"), menuIconSize, menuIconPalette),
            Systray::tr("Add account"));
        QObject::connect(addAccountAction, &QAction::triggered, addAccountAction, [] {
            closeTrayPopup();
            Systray::instance()->openAccountWizard();
        });
    }

    const auto settingsAction = addMenuAction(menu,
        templateThemeIcon(QStringLiteral("settings.svg"), menuIconSize, menuIconPalette),
        Systray::tr("Settings"));
    QObject::connect(settingsAction, &QAction::triggered, settingsAction, [] {
        openSettingsAfterTrayPopupCloses();
    });

    const auto quitAction = addMenuAction(menu,
        templateThemeIcon(QStringLiteral("close.svg"), menuIconSize, menuIconPalette),
        Systray::tr("Quit"));
    QObject::connect(quitAction, &QAction::triggered, quitAction, [] {
        closeTrayPopup();
        Systray::instance()->shutdown();
    });
}

QPoint trayPopupPosition(const QMenu *menu, const QRect &iconRect, const Systray::WindowPosition position)
{
    const auto cursorScreen = QGuiApplication::screenAt(QCursor::pos());
    const auto trayScreen = iconRect.isValid() && !iconRect.isNull()
        ? QGuiApplication::screenAt(iconRect.center())
        : nullptr;
    const auto screen = trayScreen ? trayScreen : (cursorScreen ? cursorScreen : QGuiApplication::primaryScreen());
    if (!screen) {
        return QCursor::pos();
    }

    const auto availableGeometry = screen->availableGeometry();
    const auto menuSize = menu->sizeHint();

    if (position == Systray::WindowPosition::Center) {
        const auto screenCenter = availableGeometry.center();
        return clampedMenuPosition(screenCenter - QPoint(menuSize.width() / 2, menuSize.height() / 2), menuSize, availableGeometry);
    }

    const auto trayCenter = iconRect.isValid() && !iconRect.isNull()
        ? iconRect.center()
        : QCursor::pos();
    auto positionPoint = QPoint(trayCenter.x() - menuSize.width() / 2, trayCenter.y());
    if (trayCenter.y() < availableGeometry.center().y()) {
        positionPoint.setY(availableGeometry.top());
    } else {
        positionPoint.setY(availableGeometry.bottom() - menuSize.height() + 1);
    }

    return clampedMenuPosition(positionPoint, menuSize, availableGeometry);
}

} // namespace

void setupQtTrayContextMenu(QMenu *menu, Systray *systray)
{
    if (!menu || !systray) {
        return;
    }

    populateTrayMenu(menu, systray);
    QObject::connect(menu, &QMenu::aboutToShow, menu, [menu, systray] {
        populateTrayMenu(menu, systray);
    });
}

bool showQtTrayPopup(Systray *systray, const QRect &iconRect, const Systray::WindowPosition position)
{
    if (!systray) {
        return false;
    }

    // Wayland cannot create an arbitrary QWidget popup from a tray activation.
    if (isWaylandPlatform()) {
        systray->showActivitiesWindow();
        return false;
    }

    hideQtTrayPopup();

    const auto menu = new QMenu();
    s_trayPopup = menu;
    populateTrayMenu(menu, systray);

    QObject::connect(menu, &QMenu::aboutToHide, menu, [menu, systray] {
        if (s_trayPopup == menu) {
            s_trayPopup = nullptr;
        }
        systray->setIsOpen(false);
        menu->deleteLater();
    });

    menu->popup(trayPopupPosition(menu, iconRect, position));
    return true;
}

void hideQtTrayPopup()
{
    if (s_trayPopup) {
        s_trayPopup->close();
    }
}

} // namespace OCC
