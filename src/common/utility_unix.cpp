/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"
#include "startupservicestate.h"
#include "utility.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusError>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QString>
#include <QTextStream>
#include <QtGlobal>

namespace OCC
{

using namespace Qt::StringLiterals;

namespace Utility
{
namespace
{
constexpr auto startupServiceTimeoutMs = 1000;
constexpr auto startupServiceName = LINUX_APPLICATION_ID ".service"_L1;

QDBusMessage callStartupManager(const QString &method, const QVariantList &arguments = {})
{
    auto message = QDBusMessage::createMethodCall(u"org.freedesktop.systemd1"_s, u"/org/freedesktop/systemd1"_s, u"org.freedesktop.systemd1.Manager"_s, method);
    message.setArguments(arguments);
    message.setAutoStartService(false);
    message.setInteractiveAuthorizationAllowed(false);
    return QDBusConnection::sessionBus().call(message, QDBus::Block, startupServiceTimeoutMs);
}

bool changeStartupService(const QString &method, const QVariantList &arguments, const QString &replySignature)
{
    const auto reply = callStartupManager(method, arguments);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.signature() != replySignature) {
        qCWarning(lcUtility) << "Could not change startup service:" << method << reply.errorMessage() << reply.signature();
        return false;
    }
    return true;
}

StartupServiceState startupServiceState(const QString &appName)
{
    if (appName != QLatin1String(APPLICATION_SHORTNAME) || runningInAppImage() || qEnvironmentVariableIsSet("FLATPAK_ID")
        || qEnvironmentVariableIsSet("SNAP")) {
        return {StartupServiceState::Status::NotApplicable, {}};
    }
    if (!QDBusConnection::sessionBus().isConnected()) {
        qCWarning(lcUtility) << "Could not determine startup service state: session bus is disconnected";
        return {StartupServiceState::Status::QueryFailed, {}};
    }

    const auto response = callStartupManager(u"GetUnitFileState"_s, {QString(startupServiceName)});
    const auto reply = QDBusReply<QString>(response);
    if (!reply.isValid()) {
        const auto error = reply.error();
        if (error.type() == QDBusError::ServiceUnknown || error.name() == u"org.freedesktop.DBus.Error.NameHasNoOwner"
            || error.name() == u"org.freedesktop.DBus.Error.FileNotFound" || error.name() == u"org.freedesktop.systemd1.NoSuchUnit") {
            return {StartupServiceState::Status::NotApplicable, {}};
        }
        qCWarning(lcUtility) << "Could not determine startup service state:" << error;
        return {StartupServiceState::Status::QueryFailed, {}};
    }
    if (response.signature() != u"s" || reply.value().isEmpty()) {
        qCWarning(lcUtility) << "Startup service returned an invalid unit state:" << response.signature();
        return {StartupServiceState::Status::QueryFailed, {}};
    }
    return {StartupServiceState::Status::Known, reply.value()};
}
}
}

QVector<Utility::ProcessInfosForOpenFile> Utility::queryProcessInfosKeepingFileOpen(const QString &filePath)
{
    Q_UNUSED(filePath)
    return {};
}

void Utility::setupFavLink(const QString &folder)
{
    // Nautilus: add to ~/.config/gtk-3.0/bookmarks
    QFile gtkBookmarks(QDir::homePath() + QLatin1String("/.config/gtk-3.0/bookmarks"));
    const auto folderUrl = QUrl::fromLocalFile(folder).toEncoded();
    if (!gtkBookmarks.open(QFile::ReadWrite)) {
        qCWarning(lcUtility).nospace() << "failed to set up fav link"
            << " folder=" << folder
            << " error=" << gtkBookmarks.error()
            << " errorString=" << gtkBookmarks.errorString();
        return;
    }

    auto places = gtkBookmarks.readAll();
    if (places.contains(folderUrl)) {
        qCDebug(lcUtility).nospace() << "fav link already exists"
            << " folder=" << folder
            << " folderUrl=" << folderUrl;
        return;
    }

    places += folderUrl;
    gtkBookmarks.reset();
    gtkBookmarks.write(places + '\n');
}

void Utility::removeFavLink(const QString &folder)
{
    Q_UNUSED(folder)
}

// returns the autostart directory the linux way
// and respects the XDG_CONFIG_HOME env variable
static QString getUserAutostartDir()
{
    QString config = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    config += QLatin1String("/autostart/");
    return config;
}

bool Utility::hasSystemLaunchOnStartup(const QString &appName)
{
    Q_UNUSED(appName)
    return false;
}

bool Utility::hasLaunchOnStartup(const QString &appName)
{
    const QString desktopFileLocation = getUserAutostartDir() + appName + QLatin1String(".desktop");
    const auto serviceState = startupServiceState(appName);
    // Do not report autostart disabled while the user service state is unknown.
    return QFile::exists(desktopFileLocation) || serviceState.status == StartupServiceState::Status::QueryFailed
        || (serviceState.status == StartupServiceState::Status::Known
            && (serviceState.unitFileState == u"enabled" || serviceState.unitFileState == u"enabled-runtime"));
}

void Utility::migrateFavLink(const QString &folder)
{
    Q_UNUSED(folder)
}

void Utility::setupDesktopIni(const QString &folder, const QString localizedResourceName)
{
    Q_UNUSED(folder)
    Q_UNUSED(localizedResourceName)
}

QString Utility::syncFolderDisplayName(const QString &folder, const QString &displayName)
{
    Q_UNUSED(folder)
    Q_UNUSED(displayName)
    return {};
}

void Utility::setLaunchOnStartup(const QString &appName, const QString &guiName, bool enable)
{
    const auto serviceState = startupServiceState(appName);
    if (serviceState.status == StartupServiceState::Status::QueryFailed && enable) {
        return;
    }
    if (serviceState.status == StartupServiceState::Status::Known && serviceState.unitFileState != u"masked") {
        // A user mask overrides package-wide enablement and leaves XDG autostart as the only launch path.
        const auto masked =
            changeStartupService(u"MaskUnitFiles"_s, {QStringList{startupServiceName}, false, false}, u"a(sss)"_s) && changeStartupService(u"Reload"_s, {}, {});
        if (!masked && enable) {
            const auto persistedState = startupServiceState(appName);
            if (persistedState.status != StartupServiceState::Status::Known || persistedState.unitFileState != u"masked") {
                return;
            }
        }
    }

    const auto userAutoStartPath = getUserAutostartDir();
    const QString desktopFileLocation = userAutoStartPath + appName + QLatin1String(".desktop");
    if (enable) {
        if (!QDir().exists(userAutoStartPath) && !QDir().mkpath(userAutoStartPath)) {
            qCWarning(lcUtility) << "Could not create autostart folder" << userAutoStartPath;
            return;
        }
        QFile iniFile(desktopFileLocation);
        if (!iniFile.open(QIODevice::WriteOnly)) {
            qCWarning(lcUtility) << "Could not write auto start entry" << desktopFileLocation;
            return;
        }
        // When running inside an AppImage, we need to set the path to the
        // AppImage instead of the path to the executable
        const QString appImagePath = qEnvironmentVariable("APPIMAGE");
        const bool runningInsideAppImage = !appImagePath.isNull() && QFile::exists(appImagePath);
        const QString executablePath = runningInsideAppImage ? appImagePath : QCoreApplication::applicationFilePath();

        QTextStream ts(&iniFile);
        ts << QLatin1String("[Desktop Entry]\n")
           << QLatin1String("Name=") << guiName << QLatin1Char('\n')
           << QLatin1String("GenericName=") << QLatin1String("File Synchronizer\n")
           << QLatin1String("Exec=\"") << executablePath << "\" --background\n"
           << QLatin1String("Terminal=") << "false\n"
           << QLatin1String("Icon=") << APPLICATION_ICON_NAME << QLatin1Char('\n')
           << QLatin1String("Categories=") << QLatin1String("Network\n")
           << QLatin1String("Type=") << QLatin1String("Application\n")
           << QLatin1String("StartupNotify=") << "false\n"
           << QLatin1String("X-GNOME-Autostart-enabled=") << "true\n"
           << QLatin1String("X-GNOME-Autostart-Delay=10") << Qt::endl;
    } else {
        if (!QFile::remove(desktopFileLocation)) {
            qCWarning(lcUtility) << "Could not remove autostart desktop file";
        }
    }
}

bool Utility::launchOnStartupRequiresApproval()
{
    return false;
}

bool Utility::hasDarkSystray()
{
    return true;
}

QString Utility::getCurrentUserName()
{
    return {};
}

void Utility::registerUriHandlerForLocalEditing()
{
    const auto appImagePath = qEnvironmentVariable("APPIMAGE");
    const auto runningInsideAppImage = !appImagePath.isNull() && QFile::exists(appImagePath);

    if (!runningInsideAppImage) {
        // only register x-scheme-handler if running inside appImage
        return;
    }

    // mirall.desktop.in must have an x-scheme-handler mime type specified
    const QString desktopFileName = QLatin1String(LINUX_APPLICATION_ID) + QLatin1String(".desktop");
    QProcess process;
    const QStringList args = {
        QLatin1String("default"),
        desktopFileName,
        QStringLiteral("x-scheme-handler/%1").arg(QStringLiteral(APPLICATION_URI_HANDLER_SCHEME))
    };
    process.start(QStringLiteral("xdg-mime"), args, QIODevice::ReadOnly);
    process.waitForFinished();
}

} // namespace OCC
