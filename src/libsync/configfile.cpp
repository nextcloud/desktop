/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
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

#include "config.h"

#include "common/asserts.h"
#include "common/utility.h"
#include "common/version.h"
#include "configfile.h"
#include "logger.h"
#include "theme.h"

#include "creds/abstractcredentials.h"

#include "csync_exclude.h"

#ifndef TOKEN_AUTH_ONLY
#include <QWidget>
#include <QHeaderView>
#endif

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QSettings>
#include <QNetworkProxy>
#include <QOperatingSystemVersion>
#include <QStandardPaths>

#include <chrono>
using namespace std::chrono_literals;

namespace OCC {

namespace chrono = std::chrono;

Q_LOGGING_CATEGORY(lcConfigFile, "sync.configfile", QtInfoMsg)
namespace  {
const QString logHttpC() { return QStringLiteral("logHttp"); }
const QString remotePollIntervalC() { return QStringLiteral("remotePollInterval"); }
//const QString caCertsKeyC() { return QStringLiteral("CaCertificates"); } only used from account.cpp
const QString forceSyncIntervalC() { return QStringLiteral("forceSyncInterval"); }
const QString fullLocalDiscoveryIntervalC() { return QStringLiteral("fullLocalDiscoveryInterval"); }
const QString notificationRefreshIntervalC() { return QStringLiteral("notificationRefreshInterval"); }
const QString monoIconsC() { return QStringLiteral("monoIcons"); }
const QString promptDeleteC() { return QStringLiteral("promptDeleteAllFiles"); }
const QString crashReporterC() { return QStringLiteral("crashReporter"); }
const QString optionalDesktopNoficationsC() { return QStringLiteral("optionalDesktopNotifications"); }
const QString showInExplorerNavigationPaneC() { return QStringLiteral("showInExplorerNavigationPane"); }
const QString skipUpdateCheckC() { return QStringLiteral("skipUpdateCheck"); }
const QString updateCheckIntervalC() { return QStringLiteral("updateCheckInterval"); }
const QString updateChannelC() { return QStringLiteral("updateChannel"); }
const QString uiLanguageC() { return QStringLiteral("uiLanguage"); }
const QString geometryC() { return QStringLiteral("geometry"); }
const QString timeoutC() { return QStringLiteral("timeout"); }
const QString chunkSizeC() { return QStringLiteral("chunkSize"); }
const QString minChunkSizeC() { return QStringLiteral("minChunkSize"); }
const QString maxChunkSizeC() { return QStringLiteral("maxChunkSize"); }
const QString targetChunkUploadDurationC() { return QStringLiteral("targetChunkUploadDuration"); }
const QString automaticLogDirC() { return QStringLiteral("logToTemporaryLogDir"); }
const QString numberOfLogsToKeepC() { return QStringLiteral("numberOfLogsToKeep"); }
const QString showExperimentalOptionsC() { return QStringLiteral("showExperimentalOptions"); }
const QString clientVersionC() { return QStringLiteral("clientVersion"); }

const QString proxyHostC() { return QStringLiteral("Proxy/host"); }
const QString proxyTypeC() { return QStringLiteral("Proxy/type"); }
const QString proxyPortC() { return QStringLiteral("Proxy/port"); }
const QString proxyUserC() { return QStringLiteral("Proxy/user"); }
const QString proxyPassC() { return QStringLiteral("Proxy/pass"); }
const QString proxyNeedsAuthC() { return QStringLiteral("Proxy/needsAuth"); }

const QString useUploadLimitC() { return QStringLiteral("BWLimit/useUploadLimit"); }
const QString useDownloadLimitC() { return QStringLiteral("BWLimit/useDownloadLimit"); }
const QString uploadLimitC() { return QStringLiteral("BWLimit/uploadLimit"); }
const QString downloadLimitC() { return QStringLiteral("BWLimit/downloadLimit"); }

const QString newBigFolderSizeLimitC() { return QStringLiteral("newBigFolderSizeLimit"); }
const QString useNewBigFolderSizeLimitC() { return QStringLiteral("useNewBigFolderSizeLimit"); }
const QString confirmExternalStorageC() { return QStringLiteral("confirmExternalStorage"); }
const QString moveToTrashC() { return QStringLiteral("moveToTrash"); }
}

QString ConfigFile::_confDir = QString();
const std::chrono::seconds DefaultRemotePollInterval { 30 };

static chrono::milliseconds millisecondsValue(const QSettings &setting, const QString &key,
    chrono::milliseconds defaultValue)
{
    return chrono::milliseconds(setting.value(key, qlonglong(defaultValue.count())).toLongLong());
}

ConfigFile::ConfigFile()
{
    QSettings::setDefaultFormat(QSettings::IniFormat);

    const QString config = configFile();

    QSettings settings(config, QSettings::IniFormat);
    settings.beginGroup(defaultConnection());

    // run init only once
    static bool init = [this]() {
        setLogHttp(logHttp());
        return false;
    }();
    Q_UNUSED(init);
}

bool ConfigFile::setConfDir(const QString &value)
{
    QString dirPath = value;
    if (dirPath.isEmpty())
        return false;

    QFileInfo fi(dirPath);
    if (!fi.exists()) {
        QDir().mkpath(dirPath);
        fi.setFile(dirPath);
    }
    if (fi.exists() && fi.isDir()) {
        dirPath = fi.absoluteFilePath();
        qCInfo(lcConfigFile) << "Using custom config dir " << dirPath;
        _confDir = dirPath;
        return true;
    }
    return false;
}

bool ConfigFile::optionalDesktopNotifications() const
{
    auto settings = makeQSettings();
    return settings.value(optionalDesktopNoficationsC(), true).toBool();
}

bool ConfigFile::showInExplorerNavigationPane() const
{
    auto settings = makeQSettings();
    return settings.value(showInExplorerNavigationPaneC(), QOperatingSystemVersion::current() >= QOperatingSystemVersion::Windows10).toBool();
}

void ConfigFile::setShowInExplorerNavigationPane(bool show)
{
    auto settings = makeQSettings();
    settings.setValue(showInExplorerNavigationPaneC(), show);
    settings.sync();
}

std::chrono::seconds ConfigFile::timeout() const
{
    QSettings settings(configFile(), QSettings::IniFormat);
    const auto val = settings.value(timeoutC()).toInt(); // default to 5 min
    return val ? std::chrono::seconds(val) : 5min;
}

qint64 ConfigFile::chunkSize() const
{
    auto settings = makeQSettings();
    return settings.value(chunkSizeC(), 10 * 1000 * 1000).toLongLong(); // default to 10 MB
}

qint64 ConfigFile::maxChunkSize() const
{
    auto settings = makeQSettings();
    return settings.value(maxChunkSizeC(), 100 * 1000 * 1000).toLongLong(); // default to 100 MB
}

qint64 ConfigFile::minChunkSize() const
{
    auto settings = makeQSettings();
    return settings.value(minChunkSizeC(), 1000 * 1000).toLongLong(); // default to 1 MB
}

chrono::milliseconds ConfigFile::targetChunkUploadDuration() const
{
    auto settings = makeQSettings();
    return millisecondsValue(settings, targetChunkUploadDurationC(), chrono::minutes(1));
}

void ConfigFile::setOptionalDesktopNotifications(bool show)
{
    auto settings = makeQSettings();
    settings.setValue(optionalDesktopNoficationsC(), show);
    settings.sync();
}

void ConfigFile::saveGeometry(QWidget *w)
{
#ifndef TOKEN_AUTH_ONLY
    OC_ASSERT(!w->objectName().isNull());
    auto settings = makeQSettings();
    settings.beginGroup(w->objectName());
    settings.setValue(geometryC(), w->saveGeometry());
    settings.sync();
#endif
}

void ConfigFile::restoreGeometry(QWidget *w)
{
#ifndef TOKEN_AUTH_ONLY
    w->restoreGeometry(getValue(geometryC(), w->objectName()).toByteArray());
#endif
}

void ConfigFile::saveGeometryHeader(QHeaderView *header)
{
#ifndef TOKEN_AUTH_ONLY
    if (!header)
        return;
    OC_ASSERT(!header->objectName().isEmpty());

    auto settings = makeQSettings();
    settings.beginGroup(header->objectName());
    settings.setValue(geometryC(), header->saveState());
    settings.sync();
#endif
}

void ConfigFile::restoreGeometryHeader(QHeaderView *header)
{
#ifndef TOKEN_AUTH_ONLY
    if (!header)
        return;
    OC_ASSERT(!header->objectName().isNull());

    auto settings = makeQSettings();
    settings.beginGroup(header->objectName());
    header->restoreState(settings.value(geometryC()).toByteArray());
#endif
}

QVariant ConfigFile::getPolicySetting(const QString &setting, const QVariant &defaultValue) const
{
    if (Utility::isWindows()) {
        // check for policies first and return immediately if a value is found.
        QSettings userPolicy(QStringLiteral("HKEY_CURRENT_USER\\Software\\Policies\\%1\\%2")
                                 .arg(QStringLiteral(APPLICATION_VENDOR), Theme::instance()->appNameGUI()),
            QSettings::NativeFormat);
        if (userPolicy.contains(setting)) {
            return userPolicy.value(setting);
        }

        QSettings machinePolicy(QStringLiteral("HKEY_LOCAL_MACHINE\\Software\\Policies\\%1\\%2")
                                    .arg(QStringLiteral(APPLICATION_VENDOR), Theme::instance()->appNameGUI()),
            QSettings::NativeFormat);
        if (machinePolicy.contains(setting)) {
            return machinePolicy.value(setting);
        }
    }
    return defaultValue;
}

QString ConfigFile::configPath()
{
    if (_confDir.isEmpty()) {
        // On Unix, use the AppConfigLocation for the settings, that's configurable with the XDG_CONFIG_HOME env variable.
        // On Windows, use AppDataLocation, that's where the roaming data is and where we should store the config file
        _confDir = QStandardPaths::writableLocation(Utility::isWindows() ? QStandardPaths::AppDataLocation : QStandardPaths::AppConfigLocation);
    }
    QString dir = _confDir;

    if (!dir.endsWith(QLatin1Char('/')))
        dir.append(QLatin1Char('/'));
    return dir;
}

static const QLatin1String exclFile("sync-exclude.lst");

QString ConfigFile::excludeFile(Scope scope) const
{
#ifdef Q_OS_WIN
    Utility::NtfsPermissionLookupRAII ntfs_perm;
#endif
    // prefer sync-exclude.lst, but if it does not exist, check for
    // exclude.lst for compatibility reasons in the user writeable
    // directories.
    QFileInfo fi;

    switch (scope) {
    case UserScope:
        fi.setFile(configPath(), exclFile);

        if (!fi.isReadable()) {
            fi.setFile(configPath(), QStringLiteral("exclude.lst"));
        }
        if (!fi.isReadable()) {
            fi.setFile(configPath(), exclFile);
        }
        return fi.absoluteFilePath();
    case SystemScope:
        return ConfigFile::excludeFileFromSystem();
    }

    OC_ASSERT(false);
    return QString();
}

QString ConfigFile::excludeFileFromSystem()
{
    QFileInfo fi;
#ifdef Q_OS_WIN
    fi.setFile(QCoreApplication::applicationDirPath(), exclFile);
#endif
#ifdef Q_OS_UNIX
    fi.setFile(QStringLiteral(SYSCONFDIR "/%1").arg(Theme::instance()->appName()), exclFile);
    if (!fi.exists()) {
        // Prefer to return the preferred path! Only use the fallback location
        // if the other path does not exist and the fallback is valid.
        QFileInfo nextToBinary(QCoreApplication::applicationDirPath(), exclFile);
        if (nextToBinary.exists()) {
            fi = nextToBinary;
        } else {
            // For AppImage, the file might reside under a temporary mount path
            QDir d(QCoreApplication::applicationDirPath()); // supposed to be /tmp/mount.xyz/usr/bin
            d.cdUp(); // go out of bin
            d.cdUp(); // go out of usr
            if (!d.isRoot()) { // it is really a mountpoint
                if (d.cd(QStringLiteral("etc")) && d.cd(Theme::instance()->appName())) {
                    QFileInfo inMountDir(d, exclFile);
                    if (inMountDir.exists()) {
                        fi = inMountDir;
                    }
                };
            }
        }
    }
#endif
#ifdef Q_OS_MAC
    // exec path is inside the bundle
    fi.setFile(QCoreApplication::applicationDirPath(),
        QLatin1String("../Resources/") + exclFile);
#endif

    return fi.absoluteFilePath();
}

QString ConfigFile::backup() const
{
    QString baseFile = configFile();
    auto versionString = clientVersionString();
    if (!versionString.isEmpty())
        versionString.prepend(QLatin1Char('_'));
    const QString backupFile =
        QStringLiteral("%1.backup_%2%3")
            .arg(baseFile, QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")), versionString);

    // If this exact file already exists it's most likely that a backup was
    // already done. (two backup calls directly after each other, potentially
    // even with source alterations in between!)
    if (!QFile::exists(backupFile)) {
        QFile f(baseFile);
        f.copy(backupFile);
    }
    return backupFile;
}

QString ConfigFile::configFile()
{
    return configPath() + Theme::instance()->configFileName();
}

QSettings ConfigFile::makeQSettings()
{
    return { configFile(), QSettings::IniFormat };
}

bool ConfigFile::exists()
{
    return QFileInfo::exists(configFile());
}

QString ConfigFile::defaultConnection() const
{
    return Theme::instance()->appName();
}

void ConfigFile::storeData(const QString &group, const QString &key, const QVariant &value)
{
    const QString con(group.isEmpty() ? defaultConnection() : group);
    auto settings = makeQSettings();

    settings.beginGroup(con);
    settings.setValue(key, value);
    settings.sync();
}

void ConfigFile::removeData(const QString &group, const QString &key)
{
    const QString con(group.isEmpty() ? defaultConnection() : group);
    auto settings = makeQSettings();

    settings.beginGroup(con);
    settings.remove(key);
}

bool ConfigFile::dataExists(const QString &group, const QString &key) const
{
    const QString con(group.isEmpty() ? defaultConnection() : group);
    auto settings = makeQSettings();

    settings.beginGroup(con);
    return settings.contains(key);
}

chrono::milliseconds ConfigFile::remotePollInterval(std::chrono::seconds defaultVal, const QString &connection) const
{
    QString con(connection);
    if (connection.isEmpty())
        con = defaultConnection();

    auto settings = makeQSettings();
    settings.beginGroup(con);

    auto defaultPollInterval { DefaultRemotePollInterval };

    // The server default-capabilities was set to 60 in some server releases,
    // which, if interpreted in milliseconds, is pretty small.
    // If the value is above 5 seconds, it was set intentionally.
    // Server admins have to set the value in Milliseconds!
    // i.e. set to greater than 5000 milliseconds on the server to be effective.
    if (defaultVal > chrono::seconds(5)) {
        defaultPollInterval = defaultVal;
    }
    auto remoteInterval = millisecondsValue(settings, remotePollIntervalC(), defaultPollInterval);
    if (remoteInterval < chrono::seconds(5)) {
        remoteInterval = defaultPollInterval;
        qCWarning(lcConfigFile) << "Remote Interval is less than 5 seconds, reverting to" << remoteInterval.count();
    }
    return remoteInterval;
}

void ConfigFile::setRemotePollInterval(chrono::milliseconds interval, const QString &connection)
{
    QString con(connection);
    if (connection.isEmpty())
        con = defaultConnection();

    if (interval < chrono::seconds(5)) {
        qCWarning(lcConfigFile) << "Remote Poll interval of " << interval.count() << " is below five seconds.";
        return;
    }
    auto settings = makeQSettings();
    settings.beginGroup(con);
    settings.setValue(remotePollIntervalC(), qlonglong(interval.count()));
    settings.sync();
}

chrono::milliseconds ConfigFile::forceSyncInterval(std::chrono::seconds remoteFromCapabilities, const QString &connection) const
{
    auto pollInterval = remotePollInterval(remoteFromCapabilities, connection);

    QString con(connection);
    if (connection.isEmpty())
        con = defaultConnection();
    auto settings = makeQSettings();
    settings.beginGroup(con);

    auto defaultInterval = chrono::hours(2);
    auto interval = millisecondsValue(settings, forceSyncIntervalC(), defaultInterval);
    if (interval < pollInterval) {
        qCWarning(lcConfigFile) << "Force sync interval is less than the remote poll inteval, reverting to" << pollInterval.count();
        interval = pollInterval;
    }
    return interval;
}

chrono::milliseconds OCC::ConfigFile::fullLocalDiscoveryInterval() const
{
    auto settings = makeQSettings();
    settings.beginGroup(defaultConnection());
    return millisecondsValue(settings, fullLocalDiscoveryIntervalC(), chrono::hours(1));
}

chrono::milliseconds ConfigFile::notificationRefreshInterval(const QString &connection) const
{
    QString con(connection);
    if (connection.isEmpty())
        con = defaultConnection();
    auto settings = makeQSettings();
    settings.beginGroup(con);

    auto defaultInterval = chrono::minutes(5);
    auto interval = millisecondsValue(settings, notificationRefreshIntervalC(), defaultInterval);
    if (interval < chrono::minutes(1)) {
        qCWarning(lcConfigFile) << "Notification refresh interval smaller than one minute, setting to one minute";
        interval = chrono::minutes(1);
    }
    return interval;
}

chrono::milliseconds ConfigFile::updateCheckInterval(const QString &connection) const
{
    QString con(connection);
    if (connection.isEmpty())
        con = defaultConnection();
    auto settings = makeQSettings();
    settings.beginGroup(con);

    auto defaultInterval = chrono::hours(10);
    auto interval = millisecondsValue(settings, updateCheckIntervalC(), defaultInterval);

    auto minInterval = chrono::minutes(5);
    if (interval < minInterval) {
        qCWarning(lcConfigFile) << "Update check interval less than five minutes, resetting to 5 minutes";
        interval = minInterval;
    }
    return interval;
}

bool ConfigFile::skipUpdateCheck(const QString &connection) const
{
    QString con(connection);
    if (connection.isEmpty())
        con = defaultConnection();

    QVariant fallback = getValue(skipUpdateCheckC(), con, false);
    fallback = getValue(skipUpdateCheckC(), QString(), fallback);

    QVariant value = getPolicySetting(skipUpdateCheckC(), fallback);
    return value.toBool();
}

void ConfigFile::setSkipUpdateCheck(bool skip, const QString &connection)
{
    QString con(connection);
    if (connection.isEmpty())
        con = defaultConnection();

    auto settings = makeQSettings();
    settings.beginGroup(con);

    settings.setValue(skipUpdateCheckC(), QVariant(skip));
    settings.sync();
}

QString ConfigFile::updateChannel() const
{
    QString defaultUpdateChannel = QStringLiteral("stable");
    const QString suffix = OCC::Version::suffix();
    if (suffix.startsWith(QLatin1String("daily"))
        || suffix.startsWith(QLatin1String("nightly"))
        || suffix.startsWith(QLatin1String("alpha"))
        || suffix.startsWith(QLatin1String("rc"))
        || suffix.startsWith(QLatin1String("beta"))) {
        defaultUpdateChannel = QStringLiteral("beta");
    }

    auto settings = makeQSettings();
    return settings.value(updateChannelC(), defaultUpdateChannel).toString();
}

void ConfigFile::setUpdateChannel(const QString &channel)
{
    auto settings = makeQSettings();
    settings.setValue(updateChannelC(), channel);
}

QString ConfigFile::uiLanguage() const
{
    auto settings = makeQSettings();
    return settings.value(uiLanguageC(), QString()).toString();
}

void ConfigFile::setUiLanguage(const QString &uiLanguage)
{
    auto settings = makeQSettings();
    settings.setValue(uiLanguageC(), uiLanguage);
}

void ConfigFile::setProxyType(int proxyType,
    const QString &host,
    int port, bool needsAuth,
    const QString &user,
    const QString &pass)
{
    auto settings = makeQSettings();

    settings.setValue(proxyTypeC(), proxyType);

    if (proxyType == QNetworkProxy::HttpProxy || proxyType == QNetworkProxy::Socks5Proxy) {
        settings.setValue(proxyHostC(), host);
        settings.setValue(proxyPortC(), port);
        settings.setValue(proxyNeedsAuthC(), needsAuth);
        settings.setValue(proxyUserC(), user);
        settings.setValue(proxyPassC(), pass.toUtf8().toBase64());
    }
    settings.sync();
}

QVariant ConfigFile::getValue(const QString &param, const QString &group,
    const QVariant &defaultValue) const
{
    QVariant systemSetting;
    if (Utility::isMac()) {
        QSettings systemSettings(QStringLiteral("/Library/Preferences/" APPLICATION_REV_DOMAIN ".plist"), QSettings::NativeFormat);
        if (!group.isEmpty()) {
            systemSettings.beginGroup(group);
        }
        systemSetting = systemSettings.value(param, defaultValue);
    } else if (Utility::isUnix()) {
        QSettings systemSettings(QStringLiteral(SYSCONFDIR "/%1/%1.conf").arg(Theme::instance()->appName()), QSettings::NativeFormat);
        if (!group.isEmpty()) {
            systemSettings.beginGroup(group);
        }
        systemSetting = systemSettings.value(param, defaultValue);
    } else { // Windows
        QSettings systemSettings(QStringLiteral("HKEY_LOCAL_MACHINE\\Software\\" APPLICATION_VENDOR "\\%1")
                                     .arg(Theme::instance()->appNameGUI()),
            QSettings::NativeFormat);
        if (!group.isEmpty()) {
            systemSettings.beginGroup(group);
        }
        systemSetting = systemSettings.value(param, defaultValue);
    }

    auto settings = makeQSettings();
    if (!group.isEmpty())
        settings.beginGroup(group);

    return settings.value(param, systemSetting);
}

void ConfigFile::setValue(const QString &key, const QVariant &value)
{
    auto settings = makeQSettings();

    settings.setValue(key, value);
}

int ConfigFile::proxyType() const
{
    if (Theme::instance()->forceSystemNetworkProxy()) {
        return QNetworkProxy::DefaultProxy;
    }
    return getValue(proxyTypeC()).toInt();
}

QString ConfigFile::proxyHostName() const
{
    return getValue(proxyHostC()).toString();
}

int ConfigFile::proxyPort() const
{
    return getValue(proxyPortC()).toInt();
}

bool ConfigFile::proxyNeedsAuth() const
{
    return getValue(proxyNeedsAuthC()).toBool();
}

QString ConfigFile::proxyUser() const
{
    return getValue(proxyUserC()).toString();
}

QString ConfigFile::proxyPassword() const
{
    QByteArray pass = getValue(proxyPassC()).toByteArray();
    return QString::fromUtf8(QByteArray::fromBase64(pass));
}

int ConfigFile::useUploadLimit() const
{
    return getValue(useUploadLimitC(), QString(), 0).toInt();
}

int ConfigFile::useDownloadLimit() const
{
    return getValue(useDownloadLimitC(), QString(), 0).toInt();
}

void ConfigFile::setUseUploadLimit(int val)
{
    setValue(useUploadLimitC(), val);
}

void ConfigFile::setUseDownloadLimit(int val)
{
    setValue(useDownloadLimitC(), val);
}

int ConfigFile::uploadLimit() const
{
    return getValue(uploadLimitC(), QString(), 10).toInt();
}

int ConfigFile::downloadLimit() const
{
    return getValue(downloadLimitC(), QString(), 80).toInt();
}

void ConfigFile::setUploadLimit(int kbytes)
{
    setValue(uploadLimitC(), kbytes);
}

void ConfigFile::setDownloadLimit(int kbytes)
{
    setValue(downloadLimitC(), kbytes);
}

QPair<bool, qint64> ConfigFile::newBigFolderSizeLimit() const
{
    auto defaultValue = Theme::instance()->newBigFolderSizeLimit();
    qint64 value = getValue(newBigFolderSizeLimitC(), QString(), defaultValue).toLongLong();
    bool use = value >= 0 && getValue(useNewBigFolderSizeLimitC(), QString(), true).toBool();
    return qMakePair(use, qMax<qint64>(0, value));
}

void ConfigFile::setNewBigFolderSizeLimit(bool isChecked, qint64 mbytes)
{
    setValue(newBigFolderSizeLimitC(), mbytes);
    setValue(useNewBigFolderSizeLimitC(), isChecked);
}

bool ConfigFile::confirmExternalStorage() const
{
    return getValue(confirmExternalStorageC(), QString(), true).toBool();
}

void ConfigFile::setConfirmExternalStorage(bool isChecked)
{
    setValue(confirmExternalStorageC(), isChecked);
}

bool ConfigFile::moveToTrash() const
{
    return getValue(moveToTrashC(), QString(), false).toBool();
}

void ConfigFile::setMoveToTrash(bool isChecked)
{
    setValue(moveToTrashC(), isChecked);
}

bool ConfigFile::promptDeleteFiles() const
{
    auto settings = makeQSettings();
    return settings.value(promptDeleteC(), true).toBool();
}

void ConfigFile::setPromptDeleteFiles(bool promptDeleteFiles)
{
    auto settings = makeQSettings();
    settings.setValue(promptDeleteC(), promptDeleteFiles);
}

bool ConfigFile::monoIcons() const
{
    auto settings = makeQSettings();
    bool monoDefault = false; // On Mac we want bw by default
#ifdef Q_OS_MAC
    // OEM themes are not obliged to ship mono icons
    monoDefault = (0 == (strcmp("ownCloud", APPLICATION_NAME)));
#endif
    return settings.value(monoIconsC(), monoDefault).toBool();
}

void ConfigFile::setMonoIcons(bool useMonoIcons)
{
    auto settings = makeQSettings();
    settings.setValue(monoIconsC(), useMonoIcons);
}

bool ConfigFile::crashReporter() const
{
    auto settings = makeQSettings();
    return settings.value(crashReporterC(), true).toBool();
}

void ConfigFile::setCrashReporter(bool enabled)
{
    auto settings = makeQSettings();
    settings.setValue(crashReporterC(), enabled);
}

bool ConfigFile::automaticLogDir() const
{
    auto settings = makeQSettings();
    return settings.value(automaticLogDirC(), false).toBool();
}

void ConfigFile::setAutomaticLogDir(bool enabled)
{
    auto settings = makeQSettings();
    settings.setValue(automaticLogDirC(), enabled);
}

int ConfigFile::automaticDeleteOldLogs() const
{
    auto settings = makeQSettings();
    return settings.value(numberOfLogsToKeepC()).toInt();
}

void ConfigFile::setAutomaticDeleteOldLogs(int number)
{
    auto settings = makeQSettings();
    settings.setValue(numberOfLogsToKeepC(), number);
}

void ConfigFile::setLogHttp(bool b)
{
    auto settings = makeQSettings();
    settings.setValue(logHttpC(), b);
    const QSet<QString> rule = { QStringLiteral("sync.httplogger=true") };
    if (b) {
        Logger::instance()->addLogRule(rule);
    } else {
        Logger::instance()->removeLogRule(rule);
    }
}

bool ConfigFile::logHttp() const
{
    auto settings = makeQSettings();
    return settings.value(logHttpC(), false).toBool();
}

bool ConfigFile::showExperimentalOptions() const
{
    auto settings = makeQSettings();
    return settings.value(showExperimentalOptionsC(), false).toBool();
}

QString ConfigFile::clientVersionString() const
{
    auto settings = makeQSettings();
    return settings.value(clientVersionC(), QString()).toString();
}

void ConfigFile::setClientVersionString(const QString &version)
{
    auto settings = makeQSettings();
    settings.setValue(clientVersionC(), version);
}

std::unique_ptr<QSettings> ConfigFile::settingsWithGroup(const QString &group)
{
    auto settings = std::make_unique<QSettings>(ConfigFile::configFile(), QSettings::IniFormat);
    settings->beginGroup(group);
    return settings;
}

void ConfigFile::setupDefaultExcludeFilePaths(ExcludedFiles &excludedFiles)
{
    ConfigFile cfg;
    QString systemList = cfg.excludeFile(ConfigFile::SystemScope);
    qCInfo(lcConfigFile) << "Adding system ignore list to csync:" << systemList;
    excludedFiles.addExcludeFilePath(systemList);

    QString userList = cfg.excludeFile(ConfigFile::UserScope);
    if (QFile::exists(userList)) {
        qCInfo(lcConfigFile) << "Adding user defined ignore list to csync:" << userList;
        excludedFiles.addExcludeFilePath(userList);
    }
}
}
