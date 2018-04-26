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

#include "configfile.h"
#include "theme.h"
#include "common/utility.h"
#include "common/asserts.h"
#include "version.h"

#include "creds/abstractcredentials.h"

#ifndef TOKEN_AUTH_ONLY
#include <QWidget>
#include <QHeaderView>
#include <QDesktopServices>
#endif

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QSettings>
#include <QNetworkProxy>

#define DEFAULT_REMOTE_POLL_INTERVAL 30000 // default remote poll time in milliseconds
#define DEFAULT_MAX_LOG_LINES 20000

namespace OCC {

Q_LOGGING_CATEGORY(lcConfigFile, "sync.configfile", QtInfoMsg)

//static const char caCertsKeyC[] = "CaCertificates"; only used from account.cpp
static const char remotePollIntervalC[] = "remotePollInterval";
static const char forceSyncIntervalC[] = "forceSyncInterval";
static const char notificationRefreshIntervalC[] = "notificationRefreshInterval";
static const char monoIconsC[] = "monoIcons";
static const char promptDeleteC[] = "promptDeleteAllFiles";
static const char crashReporterC[] = "crashReporter";
static const char optionalDesktopNoficationsC[] = "optionalDesktopNotifications";
static const char skipUpdateCheckC[] = "skipUpdateCheck";
static const char updateCheckIntervalC[] = "updateCheckInterval";
static const char updateChannelC[] = "updateChannel";
static const char geometryC[] = "geometry";
static const char timeoutC[] = "timeout";
static const char chunkSizeC[] = "chunkSize";
static const char minChunkSizeC[] = "minChunkSize";
static const char maxChunkSizeC[] = "maxChunkSize";
static const char targetChunkUploadDurationC[] = "targetChunkUploadDuration";

static const char proxyHostC[] = "Proxy/host";
static const char proxyTypeC[] = "Proxy/type";
static const char proxyPortC[] = "Proxy/port";
static const char proxyUserC[] = "Proxy/user";
static const char proxyPassC[] = "Proxy/pass";
static const char proxyNeedsAuthC[] = "Proxy/needsAuth";

static const char useUploadLimitC[] = "BWLimit/useUploadLimit";
static const char useDownloadLimitC[] = "BWLimit/useDownloadLimit";
static const char uploadLimitC[] = "BWLimit/uploadLimit";
static const char downloadLimitC[] = "BWLimit/downloadLimit";

static const char newBigFolderSizeLimitC[] = "newBigFolderSizeLimit";
static const char useNewBigFolderSizeLimitC[] = "useNewBigFolderSizeLimit";
static const char confirmExternalStorageC[] = "confirmExternalStorage";

static const char maxLogLinesC[] = "Logging/maxLogLines";

const char certPath[] = "http_certificatePath";
const char certPasswd[] = "http_certificatePasswd";
QString ConfigFile::_confDir = QString::null;
bool ConfigFile::_askedUser = false;

ConfigFile::ConfigFile()
{
    // QDesktopServices uses the application name to create a config path
    qApp->setApplicationName(Theme::instance()->appNameGUI());

    QSettings::setDefaultFormat(QSettings::IniFormat);

    const QString config = configFile();


    QSettings settings(config, QSettings::IniFormat);
    settings.beginGroup(defaultConnection());
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
    QSettings settings(configFile(), QSettings::IniFormat);
    return settings.value(QLatin1String(optionalDesktopNoficationsC), true).toBool();
}

int ConfigFile::timeout() const
{
    QSettings settings(configFile(), QSettings::IniFormat);
    return settings.value(QLatin1String(timeoutC), 300).toInt(); // default to 5 min
}

quint64 ConfigFile::chunkSize() const
{
    QSettings settings(configFile(), QSettings::IniFormat);
    return settings.value(QLatin1String(chunkSizeC), 10 * 1000 * 1000).toLongLong(); // default to 10 MB
}

quint64 ConfigFile::maxChunkSize() const
{
    QSettings settings(configFile(), QSettings::IniFormat);
    return settings.value(QLatin1String(maxChunkSizeC), 100 * 1000 * 1000).toLongLong(); // default to 100 MB
}

quint64 ConfigFile::minChunkSize() const
{
    QSettings settings(configFile(), QSettings::IniFormat);
    return settings.value(QLatin1String(minChunkSizeC), 1000 * 1000).toLongLong(); // default to 1 MB
}

quint64 ConfigFile::targetChunkUploadDuration() const
{
    QSettings settings(configFile(), QSettings::IniFormat);
    return settings.value(QLatin1String(targetChunkUploadDurationC), 60 * 1000).toLongLong(); // default to 1 minute
}

void ConfigFile::setOptionalDesktopNotifications(bool show)
{
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setValue(QLatin1String(optionalDesktopNoficationsC), show);
    settings.sync();
}

void ConfigFile::saveGeometry(QWidget *w)
{
#ifndef TOKEN_AUTH_ONLY
    ASSERT(!w->objectName().isNull());
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.beginGroup(w->objectName());
    settings.setValue(QLatin1String(geometryC), w->saveGeometry());
    settings.sync();
#endif
}

void ConfigFile::restoreGeometry(QWidget *w)
{
#ifndef TOKEN_AUTH_ONLY
    w->restoreGeometry(getValue(geometryC, w->objectName()).toByteArray());
#endif
}

void ConfigFile::saveGeometryHeader(QHeaderView *header)
{
#ifndef TOKEN_AUTH_ONLY
    if (!header)
        return;
    ASSERT(!header->objectName().isEmpty());

    QSettings settings(configFile(), QSettings::IniFormat);
    settings.beginGroup(header->objectName());
    settings.setValue(QLatin1String(geometryC), header->saveState());
    settings.sync();
#endif
}

void ConfigFile::restoreGeometryHeader(QHeaderView *header)
{
#ifndef TOKEN_AUTH_ONLY
    if (!header)
        return;
    ASSERT(!header->objectName().isNull());

    QSettings settings(configFile(), QSettings::IniFormat);
    settings.beginGroup(header->objectName());
    header->restoreState(settings.value(geometryC).toByteArray());
#endif
}

QVariant ConfigFile::getPolicySetting(const QString &setting, const QVariant &defaultValue) const
{
    if (Utility::isWindows()) {
        // check for policies first and return immediately if a value is found.
        QSettings userPolicy(QString::fromLatin1("HKEY_CURRENT_USER\\Software\\Policies\\%1\\%2")
                                 .arg(APPLICATION_VENDOR, Theme::instance()->appName()),
            QSettings::NativeFormat);
        if (userPolicy.contains(setting)) {
            return userPolicy.value(setting);
        }

        QSettings machinePolicy(QString::fromLatin1("HKEY_LOCAL_MACHINE\\Software\\Policies\\%1\\%2")
                                    .arg(APPLICATION_VENDOR, APPLICATION_NAME),
            QSettings::NativeFormat);
        if (machinePolicy.contains(setting)) {
            return machinePolicy.value(setting);
        }
    }
    return defaultValue;
}

QString ConfigFile::configPath() const
{
#ifndef TOKEN_AUTH_ONLY
    if (_confDir.isEmpty()) {
        //  Qt 5's QStandardPaths::writableLocation gives us wrong results (without /data/),
        //  so we'll have to use the deprecated version for now
        _confDir = QDesktopServices::storageLocation(QDesktopServices::DataLocation);
    }
#endif
    QString dir = _confDir;

    if (!dir.endsWith(QLatin1Char('/')))
        dir.append(QLatin1Char('/'));
    return dir;
}

QString ConfigFile::configPathWithAppName() const
{
    //HACK
    return QFileInfo(configFile()).dir().absolutePath().append("/");
}

static const QLatin1String exclFile("sync-exclude.lst");

QString ConfigFile::excludeFile(Scope scope) const
{
    // prefer sync-exclude.lst, but if it does not exist, check for
    // exclude.lst for compatibility reasons in the user writeable
    // directories.
    QFileInfo fi;

    switch (scope) {
    case UserScope:
        fi.setFile(configPath(), exclFile);

        if (!fi.isReadable()) {
            fi.setFile(configPath(), QLatin1String("exclude.lst"));
        }
        if (!fi.isReadable()) {
            fi.setFile(configPath(), exclFile);
        }
        return fi.absoluteFilePath();
    case SystemScope:
        return ConfigFile::excludeFileFromSystem();
    }

    ASSERT(false);
    return QString();
}

QString ConfigFile::excludeFileFromSystem()
{
    QFileInfo fi;
#ifdef Q_OS_WIN
    fi.setFile(QCoreApplication::applicationDirPath(), exclFile);
#endif
#ifdef Q_OS_UNIX
    fi.setFile(QString(SYSCONFDIR "/" + Theme::instance()->appName()), exclFile);
    if (!fi.exists()) {
        // Prefer to return the preferred path! Only use the fallback location
        // if the other path does not exist and the fallback is valid.
        QFileInfo nextToBinary(QCoreApplication::applicationDirPath(), exclFile);
        if (nextToBinary.exists()) {
            fi = nextToBinary;
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

QString ConfigFile::configFile() const
{
    return configPath() + Theme::instance()->configFileName();
}

bool ConfigFile::exists()
{
    QFile file(configFile());
    return file.exists();
}

QString ConfigFile::defaultConnection() const
{
    return Theme::instance()->appName();
}

void ConfigFile::storeData(const QString &group, const QString &key, const QVariant &value)
{
    const QString con(group.isEmpty() ? defaultConnection() : group);
    QSettings settings(configFile(), QSettings::IniFormat);

    settings.beginGroup(con);
    settings.setValue(key, value);
    settings.sync();
}

QVariant ConfigFile::retrieveData(const QString &group, const QString &key) const
{
    const QString con(group.isEmpty() ? defaultConnection() : group);
    QSettings settings(configFile(), QSettings::IniFormat);

    settings.beginGroup(con);
    return settings.value(key);
}

void ConfigFile::removeData(const QString &group, const QString &key)
{
    const QString con(group.isEmpty() ? defaultConnection() : group);
    QSettings settings(configFile(), QSettings::IniFormat);

    settings.beginGroup(con);
    settings.remove(key);
}

bool ConfigFile::dataExists(const QString &group, const QString &key) const
{
    const QString con(group.isEmpty() ? defaultConnection() : group);
    QSettings settings(configFile(), QSettings::IniFormat);

    settings.beginGroup(con);
    return settings.contains(key);
}

int ConfigFile::remotePollInterval(const QString &connection) const
{
    QString con(connection);
    if (connection.isEmpty())
        con = defaultConnection();

    QSettings settings(configFile(), QSettings::IniFormat);
    settings.beginGroup(con);

    int remoteInterval = settings.value(QLatin1String(remotePollIntervalC), DEFAULT_REMOTE_POLL_INTERVAL).toInt();
    if (remoteInterval < 5000) {
        qCWarning(lcConfigFile) << "Remote Interval is less than 5 seconds, reverting to" << DEFAULT_REMOTE_POLL_INTERVAL;
        remoteInterval = DEFAULT_REMOTE_POLL_INTERVAL;
    }
    return remoteInterval;
}

void ConfigFile::setRemotePollInterval(int interval, const QString &connection)
{
    QString con(connection);
    if (connection.isEmpty())
        con = defaultConnection();

    if (interval < 5000) {
        qCWarning(lcConfigFile) << "Remote Poll interval of " << interval << " is below five seconds.";
        return;
    }
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.beginGroup(con);
    settings.setValue(QLatin1String(remotePollIntervalC), interval);
    settings.sync();
}

quint64 ConfigFile::forceSyncInterval(const QString &connection) const
{
    uint pollInterval = remotePollInterval(connection);

    QString con(connection);
    if (connection.isEmpty())
        con = defaultConnection();
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.beginGroup(con);

    quint64 defaultInterval = 2 * 60 * 60 * 1000ull; // 2h
    quint64 interval = settings.value(QLatin1String(forceSyncIntervalC), defaultInterval).toULongLong();
    if (interval < pollInterval) {
        qCWarning(lcConfigFile) << "Force sync interval is less than the remote poll inteval, reverting to" << pollInterval;
        interval = pollInterval;
    }
    return interval;
}

quint64 ConfigFile::notificationRefreshInterval(const QString &connection) const
{
    QString con(connection);
    if (connection.isEmpty())
        con = defaultConnection();
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.beginGroup(con);

    quint64 defaultInterval = 5 * 60 * 1000ull; // 5 minutes
    quint64 interval = settings.value(QLatin1String(notificationRefreshIntervalC), defaultInterval).toULongLong();
    if (interval < 60 * 1000ull) {
        qCWarning(lcConfigFile) << "Notification refresh interval smaller than one minute, setting to one minute";
        interval = 60 * 1000ull;
    }
    return interval;
}

int ConfigFile::updateCheckInterval(const QString &connection) const
{
    QString con(connection);
    if (connection.isEmpty())
        con = defaultConnection();
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.beginGroup(con);

    int defaultInterval = 1000 * 60 * 60 * 10; // ten hours
    int interval = settings.value(QLatin1String(updateCheckIntervalC), defaultInterval).toInt();

    int minInterval = 1000 * 60 * 5;
    if (interval < minInterval) {
        qCWarning(lcConfigFile) << "Update check interval less than five minutes, setting " << minInterval;
        interval = minInterval;
    }
    return interval;
}

bool ConfigFile::skipUpdateCheck(const QString &connection) const
{
    QString con(connection);
    if (connection.isEmpty())
        con = defaultConnection();

    QVariant fallback = getValue(QLatin1String(skipUpdateCheckC), con, false);
    fallback = getValue(QLatin1String(skipUpdateCheckC), QString(), fallback);

    QVariant value = getPolicySetting(QLatin1String(skipUpdateCheckC), fallback);
    return value.toBool();
}

void ConfigFile::setSkipUpdateCheck(bool skip, const QString &connection)
{
    QString con(connection);
    if (connection.isEmpty())
        con = defaultConnection();

    QSettings settings(configFile(), QSettings::IniFormat);
    settings.beginGroup(con);

    settings.setValue(QLatin1String(skipUpdateCheckC), QVariant(skip));
    settings.sync();
}

QString ConfigFile::updateChannel() const
{
    QString defaultUpdateChannel = QStringLiteral("stable");
    QString suffix = QString::fromLatin1(MIRALL_STRINGIFY(MIRALL_VERSION_SUFFIX));
    if (suffix.startsWith("daily")
        || suffix.startsWith("nightly")
        || suffix.startsWith("alpha")
        || suffix.startsWith("rc")
        || suffix.startsWith("beta")) {
        defaultUpdateChannel = QStringLiteral("beta");
    }

    QSettings settings(configFile(), QSettings::IniFormat);
    return settings.value(QLatin1String(updateChannelC), defaultUpdateChannel).toString();
}

void ConfigFile::setUpdateChannel(const QString &channel)
{
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setValue(QLatin1String(updateChannelC), channel);
}

int ConfigFile::maxLogLines() const
{
    QSettings settings(configFile(), QSettings::IniFormat);
    return settings.value(QLatin1String(maxLogLinesC), DEFAULT_MAX_LOG_LINES).toInt();
}

void ConfigFile::setMaxLogLines(int lines)
{
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setValue(QLatin1String(maxLogLinesC), lines);
    settings.sync();
}

void ConfigFile::setProxyType(int proxyType,
    const QString &host,
    int port, bool needsAuth,
    const QString &user,
    const QString &pass)
{
    QSettings settings(configFile(), QSettings::IniFormat);

    settings.setValue(QLatin1String(proxyTypeC), proxyType);

    if (proxyType == QNetworkProxy::HttpProxy || proxyType == QNetworkProxy::Socks5Proxy) {
        settings.setValue(QLatin1String(proxyHostC), host);
        settings.setValue(QLatin1String(proxyPortC), port);
        settings.setValue(QLatin1String(proxyNeedsAuthC), needsAuth);
        settings.setValue(QLatin1String(proxyUserC), user);
        settings.setValue(QLatin1String(proxyPassC), pass.toUtf8().toBase64());
    }
    settings.sync();
}

QVariant ConfigFile::getValue(const QString &param, const QString &group,
    const QVariant &defaultValue) const
{
    QVariant systemSetting;
    if (Utility::isMac()) {
        QSettings systemSettings(QLatin1String("/Library/Preferences/" APPLICATION_REV_DOMAIN ".plist"), QSettings::NativeFormat);
        if (!group.isEmpty()) {
            systemSettings.beginGroup(group);
        }
        systemSetting = systemSettings.value(param, defaultValue);
    } else if (Utility::isUnix()) {
        QSettings systemSettings(QString(SYSCONFDIR "/%1/%1.conf").arg(Theme::instance()->appName()), QSettings::NativeFormat);
        if (!group.isEmpty()) {
            systemSettings.beginGroup(group);
        }
        systemSetting = systemSettings.value(param, defaultValue);
    } else { // Windows
        QSettings systemSettings(QString::fromLatin1("HKEY_LOCAL_MACHINE\\Software\\%1\\%2")
                                     .arg(APPLICATION_VENDOR, Theme::instance()->appName()),
            QSettings::NativeFormat);
        if (!group.isEmpty()) {
            systemSettings.beginGroup(group);
        }
        systemSetting = systemSettings.value(param, defaultValue);
    }

    QSettings settings(configFile(), QSettings::IniFormat);
    if (!group.isEmpty())
        settings.beginGroup(group);

    return settings.value(param, systemSetting);
}

void ConfigFile::setValue(const QString &key, const QVariant &value)
{
    QSettings settings(configFile(), QSettings::IniFormat);

    settings.setValue(key, value);
}

int ConfigFile::proxyType() const
{
    if (Theme::instance()->forceSystemNetworkProxy()) {
        return QNetworkProxy::DefaultProxy;
    }
    return getValue(QLatin1String(proxyTypeC)).toInt();
}

QString ConfigFile::proxyHostName() const
{
    return getValue(QLatin1String(proxyHostC)).toString();
}

int ConfigFile::proxyPort() const
{
    return getValue(QLatin1String(proxyPortC)).toInt();
}

bool ConfigFile::proxyNeedsAuth() const
{
    return getValue(QLatin1String(proxyNeedsAuthC)).toBool();
}

QString ConfigFile::proxyUser() const
{
    return getValue(QLatin1String(proxyUserC)).toString();
}

QString ConfigFile::proxyPassword() const
{
    QByteArray pass = getValue(proxyPassC).toByteArray();
    return QString::fromUtf8(QByteArray::fromBase64(pass));
}

int ConfigFile::useUploadLimit() const
{
    return getValue(useUploadLimitC, QString::null, 0).toInt();
}

int ConfigFile::useDownloadLimit() const
{
    return getValue(useDownloadLimitC, QString::null, 0).toInt();
}

void ConfigFile::setUseUploadLimit(int val)
{
    setValue(useUploadLimitC, val);
}

void ConfigFile::setUseDownloadLimit(int val)
{
    setValue(useDownloadLimitC, val);
}

int ConfigFile::uploadLimit() const
{
    return getValue(uploadLimitC, QString::null, 10).toInt();
}

int ConfigFile::downloadLimit() const
{
    return getValue(downloadLimitC, QString::null, 80).toInt();
}

void ConfigFile::setUploadLimit(int kbytes)
{
    setValue(uploadLimitC, kbytes);
}

void ConfigFile::setDownloadLimit(int kbytes)
{
    setValue(downloadLimitC, kbytes);
}

QPair<bool, quint64> ConfigFile::newBigFolderSizeLimit() const
{
    auto defaultValue = Theme::instance()->newBigFolderSizeLimit();
    qint64 value = getValue(newBigFolderSizeLimitC, QString(), defaultValue).toLongLong();
    bool use = value >= 0 && getValue(useNewBigFolderSizeLimitC, QString(), true).toBool();
    return qMakePair(use, quint64(qMax<qint64>(0, value)));
}

void ConfigFile::setNewBigFolderSizeLimit(bool isChecked, quint64 mbytes)
{
    setValue(newBigFolderSizeLimitC, mbytes);
    setValue(useNewBigFolderSizeLimitC, isChecked);
}

bool ConfigFile::confirmExternalStorage() const
{
    return getValue(confirmExternalStorageC, QString(), true).toBool();
}

void ConfigFile::setConfirmExternalStorage(bool isChecked)
{
    setValue(confirmExternalStorageC, isChecked);
}

bool ConfigFile::promptDeleteFiles() const
{
    QSettings settings(configFile(), QSettings::IniFormat);
    return settings.value(QLatin1String(promptDeleteC), true).toBool();
}

void ConfigFile::setPromptDeleteFiles(bool promptDeleteFiles)
{
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setValue(QLatin1String(promptDeleteC), promptDeleteFiles);
}

bool ConfigFile::monoIcons() const
{
    QSettings settings(configFile(), QSettings::IniFormat);
    bool monoDefault = false; // On Mac we want bw by default
#ifdef Q_OS_MAC
    // OEM themes are not obliged to ship mono icons
    monoDefault = (0 == (strcmp("ownCloud", APPLICATION_NAME)));
#endif
    return settings.value(QLatin1String(monoIconsC), monoDefault).toBool();
}

void ConfigFile::setMonoIcons(bool useMonoIcons)
{
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setValue(QLatin1String(monoIconsC), useMonoIcons);
}

bool ConfigFile::crashReporter() const
{
    QSettings settings(configFile(), QSettings::IniFormat);
    return settings.value(QLatin1String(crashReporterC), true).toBool();
}

void ConfigFile::setCrashReporter(bool enabled)
{
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setValue(QLatin1String(crashReporterC), enabled);
}

QString ConfigFile::certificatePath() const
{
    return retrieveData(QString(), QLatin1String(certPath)).toString();
}

void ConfigFile::setCertificatePath(const QString &cPath)
{
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setValue(QLatin1String(certPath), cPath);
    settings.sync();
}

QString ConfigFile::certificatePasswd() const
{
    return retrieveData(QString(), QLatin1String(certPasswd)).toString();
}

void ConfigFile::setCertificatePasswd(const QString &cPasswd)
{
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setValue(QLatin1String(certPasswd), cPasswd);
    settings.sync();
}

Q_GLOBAL_STATIC(QString, g_configFileName)

std::unique_ptr<QSettings> ConfigFile::settingsWithGroup(const QString &group, QObject *parent)
{
    if (g_configFileName()->isEmpty()) {
        // cache file name
        ConfigFile cfg;
        *g_configFileName() = cfg.configFile();
    }
    std::unique_ptr<QSettings> settings(new QSettings(*g_configFileName(), QSettings::IniFormat, parent));
    settings->beginGroup(group);
    return settings;
}

}
