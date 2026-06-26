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
#include "tray/usermodel.h"

#include <QAction>
#include <QColor>
#include <QCoreApplication>
#include <QCursor>
#include <QGuiApplication>
#include <QHash>
#include <QIcon>
#include <QImage>
#include <QMenu>
#include <QMimeDatabase>
#include <QMimeType>
#include <QPalette>
#include <QPainter>
#include <QPointer>
#include <QPixmap>
#include <QScreen>
#include <QSvgRenderer>
#include <QUrl>
#include <QVariantMap>

namespace OCC {

// Keep behavior and menu taxonomy aligned with src/gui/macOS/trayaccountpopup_mac.mm.

namespace {

QPointer<QMenu> s_trayPopup;
QHash<QString, QIcon> s_remoteAppIconCache;

QRectF aspectFitRect(const QSize &sourceSize, const QSize &targetSize)
{
    if (!sourceSize.isValid() || !targetSize.isValid()) {
        return QRectF(QPointF(0.0, 0.0), targetSize);
    }

    const auto scaledSize = sourceSize.scaled(targetSize, Qt::KeepAspectRatio);
    return QRectF(QPointF((targetSize.width() - scaledSize.width()) / 2.0,
                          (targetSize.height() - scaledSize.height()) / 2.0),
                  scaledSize);
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

QString remoteIconCacheKey(const AccountStatePtr &accountState, const QUrl &url, const QColor &iconColor)
{
    if (!accountState || !accountState->account()) {
        return {};
    }
    return QStringLiteral("%1:%2:%3").arg(
        accountState->account()->id(),
        iconColor.name(QColor::HexArgb),
        url.toString());
}

void fetchRemoteAppIcon(QAction *action,
    const AccountStatePtr &accountState,
    const QUrl &iconUrl,
    const QColor &iconColor)
{
    if (!action || !accountState || !accountState->account() || !isRemoteIconUrl(iconUrl)) {
        return;
    }

    const auto cacheKey = remoteIconCacheKey(accountState, iconUrl, iconColor);
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
        [actionPointer, cacheKey, iconColor](const QByteArray &iconData) {
            const auto image = imageFromImageData(iconData, QSize(18, 18));
            if (image.isNull()) {
                return;
            }

            const auto icon = QIcon(QPixmap::fromImage(tintImage(image, iconColor)));
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

QString titleWithDetail(const QString &title, const QString &subtitle, const QString &dateTime)
{
    auto result = title;
    if (!subtitle.isEmpty()) {
        result = QStringLiteral("%1 - %2").arg(result, subtitle);
    }
    if (!dateTime.isEmpty()) {
        result = QStringLiteral("%1 (%2)").arg(result, dateTime);
    }
    return result;
}

QAction *addMenuAction(QMenu *menu, const QIcon &icon, const QString &text)
{
    return icon.isNull() ? menu->addAction(text) : menu->addAction(icon, text);
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

void openNotification(const int userId, const bool opensSettings)
{
    closeTrayPopup();
    if (opensSettings) {
        Systray::instance()->openSettings();
        return;
    }
    Systray::instance()->showActivitiesWindow(userId);
}

bool populateAppsMenu(QMenu *menu, const int userId)
{
    menu->clear();

    const auto appsModel = TrayAccountAppsModel::instance();
    appsModel->setUserId(userId);
    const auto appCount = appsModel->rowCount();
    const auto accounts = AccountManager::instance()->accounts();
    const auto accountState = userId >= 0 && userId < accounts.size()
        ? accounts.at(userId)
        : AccountStatePtr{};
    const auto appIconColor = menu->palette().color(QPalette::Active, QPalette::Text);

    for (auto row = 0; row < appCount; ++row) {
        const auto appIndex = appsModel->index(row);
        const auto appName = appsModel->data(appIndex, TrayAccountAppsModel::NameRole).toString();
        const auto appUrl = appsModel->data(appIndex, TrayAccountAppsModel::UrlRole).toUrl();
        const auto appIconUrl = appsModel->data(appIndex, TrayAccountAppsModel::IconUrlRole).toUrl();
        auto appIcon = iconFromUrl(appIconUrl);
        if (appIcon.isNull()) {
            appIcon = blackThemeIcon(QStringLiteral("more-apps.svg"));
        }

        const auto action = addMenuAction(menu, appIcon, appName);
        fetchRemoteAppIcon(action, accountState, appIconUrl, appIconColor);
        QObject::connect(action, &QAction::triggered, action, [appUrl] {
            closeTrayPopup();
            TrayAccountAppsModel::instance()->openAppUrl(appUrl);
        });
    }

    if (menu->isEmpty()) {
        const auto noAppsAction = menu->addAction(QCoreApplication::translate("TrayAccountPopup", "No apps available"));
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
        const auto action = menu->addAction(title);
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

    menu->addSection(QCoreApplication::translate("TrayAccountPopup", "Notifications"));
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
        const auto menuText = titleWithDetail(title, {}, dateTime);
        if (actions.isEmpty()) {
            const auto action = addMenuAction(menu, activityIcon(notificationData), menuText);
            QObject::connect(action, &QAction::triggered, action, [userId, opensSettings] {
                openNotification(userId, opensSettings);
            });
            continue;
        }

        const auto notificationMenu = menu->addMenu(activityIcon(notificationData), menuText);
        const auto openAction = notificationMenu->addAction(QCoreApplication::translate("TrayAccountPopup", "Open"));
        QObject::connect(openAction, &QAction::triggered, openAction, [userId, opensSettings] {
            openNotification(userId, opensSettings);
        });
        notificationMenu->addSeparator();
        populateNotificationActionsMenu(notificationMenu, userId, activityIndex, actions);
    }
}

void addRecentActivities(QMenu *menu, const int userId, const QVariantList &recentActivities)
{
    menu->addSection(QCoreApplication::translate("TrayAccountPopup", "Recent activity"));

    if (recentActivities.isEmpty()) {
        const auto noRecentActivity = addMenuAction(menu,
            blackThemeIcon(QStringLiteral("activity.svg")),
            QCoreApplication::translate("TrayAccountPopup", "No recent activity"));
        noRecentActivity->setEnabled(false);
    }

    for (const auto &recentActivityVariant : recentActivities) {
        const auto activityData = recentActivityVariant.toMap();
        const auto title = activityData.value(QStringLiteral("title")).toString();
        if (title.isEmpty()) {
            continue;
        }

        const auto subtitle = activityData.value(QStringLiteral("subtitle")).toString();
        const auto dateTime = activityData.value(QStringLiteral("dateTime")).toString();
        const auto action = addMenuAction(menu, activityIcon(activityData), titleWithDetail(title, subtitle, dateTime));
        QObject::connect(action, &QAction::triggered, action, [userId] {
            openActivitiesForUser(userId);
        });
    }

    const auto moreActivitiesAction = menu->addAction(QCoreApplication::translate("TrayAccountPopup", "More activity\342\200\246"));
    QObject::connect(moreActivitiesAction, &QAction::triggered, moreActivitiesAction, [userId] {
        openActivitiesForUser(userId);
    });
}

void populateAccountMenu(QMenu *menu, const int userId)
{
    menu->clear();

    const auto userModel = UserModel::instance();
    if (!userModel || userId < 0 || userId >= userModel->rowCount()) {
        return;
    }

    userModel->fetchActivityPreview(userId);

    const auto userModelIndex = userModel->index(userId);
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
        menu->addSection(QCoreApplication::translate("TrayAccountPopup", "User status"));
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
        themeIcon(QStringLiteral("file-open.svg")),
        QCoreApplication::translate("TrayFoldersMenuButton", "Open local folder"));
    QObject::connect(openFolderAction, &QAction::triggered, openFolderAction, [userId] {
        openLocalFolderForUser(userId);
    });

    const auto assistantEnabled = userModel->data(userModelIndex, UserModel::AssistantEnabledRole).toBool();
    if (assistantEnabled) {
        const auto assistantAction = addMenuAction(menu,
            blackThemeIcon(QStringLiteral("nc-assistant-app.svg")),
            QCoreApplication::translate("MainWindow", "Ask Assistant\302\240\342\200\246"));
        QObject::connect(assistantAction, &QAction::triggered, assistantAction, [userId] {
            openAssistantForUser(userId);
        });
    }

    const auto appsMenu = menu->addMenu(blackThemeIcon(QStringLiteral("more-apps.svg")),
        QCoreApplication::translate("TrayWindowHeader", "More apps"));
    appsMenu->menuAction()->setEnabled(populateAppsMenu(appsMenu, userId));
    QObject::connect(appsMenu, &QMenu::aboutToShow, appsMenu, [appsMenu, userId] {
        appsMenu->menuAction()->setEnabled(populateAppsMenu(appsMenu, userId));
    });

    menu->addSeparator();

    addNotifications(menu, userId, userModel->data(userModelIndex, UserModel::TrayNotificationsRole).toList());
    addRecentActivities(menu, userId, userModel->data(userModelIndex, UserModel::RecentActivitiesRole).toList());
}

void populateTrayMenu(QMenu *menu)
{
    menu->clear();

    const auto userModel = UserModel::instance();
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

            const auto accountMenu = menu->addMenu(accountIcon, accountText);
            QObject::connect(accountMenu, &QMenu::aboutToShow, accountMenu, [accountMenu, userId] {
                populateAccountMenu(accountMenu, userId);
            });
        }
        menu->addSeparator();
    }

    if (Systray::instance()->enableAddAccount()) {
        const auto addAccountAction = addMenuAction(menu, themeIcon(QStringLiteral("add.svg")), Systray::tr("Add account"));
        QObject::connect(addAccountAction, &QAction::triggered, addAccountAction, [] {
            closeTrayPopup();
            Systray::instance()->openAccountWizard();
        });
    }

    const auto settingsAction = addMenuAction(menu, themeIcon(QStringLiteral("settings.svg")), Systray::tr("Settings"));
    QObject::connect(settingsAction, &QAction::triggered, settingsAction, [] {
        closeTrayPopup();
        Systray::instance()->openSettings();
    });

    const auto quitAction = addMenuAction(menu, themeIcon(QStringLiteral("close.svg")), Systray::tr("Quit"));
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

    auto positionPoint = QCursor::pos();
    if (iconRect.isValid() && !iconRect.isNull()) {
        const auto trayCenter = iconRect.center();
        positionPoint.setX(trayCenter.x() - menuSize.width() / 2);
        if (trayCenter.y() < availableGeometry.center().y()) {
            positionPoint.setY(availableGeometry.top());
        } else {
            positionPoint.setY(availableGeometry.bottom() - menuSize.height() + 1);
        }
    }

    return clampedMenuPosition(positionPoint, menuSize, availableGeometry);
}

} // namespace

void showQtTrayPopup(const QRect &iconRect, const Systray::WindowPosition position)
{
    hideQtTrayPopup();

    const auto menu = new QMenu();
    s_trayPopup = menu;
    populateTrayMenu(menu);

    QObject::connect(menu, &QMenu::aboutToHide, menu, [menu] {
        if (s_trayPopup == menu) {
            s_trayPopup = nullptr;
        }
        Systray::instance()->setIsOpen(false);
        menu->deleteLater();
    });

    menu->popup(trayPopupPosition(menu, iconRect, position));
}

void hideQtTrayPopup()
{
    if (s_trayPopup) {
        s_trayPopup->close();
    }
}

} // namespace OCC
