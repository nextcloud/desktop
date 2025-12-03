/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "configfile.h"

#include "common/asserts.h"
#include "common/utility.h"
#include "config.h"
#include "creds/keychainchunk.h"
#include "csync_exclude.h"
#include "theme.h"
#include "updatechannel.h"
#include "version.h"

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
#include <QStandardPaths>
#include <QOperatingSystemVersion>

#define DEFAULT_REMOTE_POLL_INTERVAL 30000
#define DEFAULT_MAX_LOG_LINES 20000

namespace {
static constexpr char showMainDialogAsNormalWindowC[] = "showMainDialogAsNormalWindow";
static constexpr char showConfigBackupWarningC[] = "showConfigBackupWarning";
static constexpr char remotePollIntervalC[] = "remotePollInterval";
static constexpr char forceSyncIntervalC[] = "forceSyncInterval";
static constexpr char fullLocalDiscoveryIntervalC[] = "fullLocalDiscoveryInterval";
static constexpr char notificationRefreshIntervalC[] = "notificationRefreshInterval";
static constexpr char deleteFilesThresholdC[] = "deleteFilesThreshold";
static constexpr char skipUpdateCheckC[] = "skipUpdateCheck";
static constexpr char updateCheckIntervalC[] = "updateCheckInterval";
static constexpr char updateSegmentC[] = "updateSegment";
static constexpr char overrideServerUrlC[] = "overrideServerUrl";
static constexpr char overrideLocalDirC[] = "overrideLocalDir";
static constexpr char geometryC[] = "geometry";
static constexpr char timeoutC[] = "timeout";
static constexpr char chunkSizeC[] = "chunkSize";
static constexpr char minChunkSizeC[] = "minChunkSize";
static constexpr char maxChunkSizeC[] = "maxChunkSize";
static constexpr char targetChunkUploadDurationC[] = "targetChunkUploadDuration";
static constexpr char automaticLogDirC[] = "logToTemporaryLogDir";
static constexpr char logDirC[] = "logDir";
static constexpr char logDebugC[] = "logDebug";
static constexpr char logExpireC[] = "logExpire";
static constexpr char logFlushC[] = "logFlush";
static constexpr char showExperimentalOptionsC[] = "showExperimentalOptions";
static constexpr char clientPreviousVersionC[] = "clientPreviousVersion";

static constexpr char proxyHostC[] = "Proxy/host";
static constexpr char proxyTypeC[] = "Proxy/type";
static constexpr char proxyPortC[] = "Proxy/port";
static constexpr char proxyUserC[] = "Proxy/user";
static constexpr char proxyPassC[] = "Proxy/pass";
static constexpr char proxyNeedsAuthC[] = "Proxy/needsAuth";
static constexpr char forceLoginV2C[] = "forceLoginV2";

static constexpr char certPath[] = "http_certificatePath";
static constexpr char certPasswd[] = "http_certificatePasswd";

static constexpr char serverHasValidSubscriptionC[] = "serverHasValidSubscription";
static constexpr char desktopEnterpriseChannelName[] = "desktopEnterpriseChannel";

static constexpr char languageC[] = "language";

static constexpr char lastSelectedAccountC[] = "lastSelectedAccount";

static constexpr int deleteFilesThresholdDefaultValue = 100;
}

namespace OCC {

namespace chrono = std::chrono;

Q_LOGGING_CATEGORY(lcConfigFile, "nextcloud.sync.configfile", QtInfoMsg)

QString ConfigFile::_confDir = {};
QString ConfigFile::_discoveredLegacyConfigPath = {};
ConfigFile::MigrationPhase ConfigFile::_migrationPhase = ConfigFile::MigrationPhase::NotStarted;

static chrono::milliseconds millisecondsValue(const QSettings &setting, const char *key,
    chrono::milliseconds defaultValue)
{
    return chrono::milliseconds(setting.value(QLatin1String(key), qlonglong(defaultValue.count())).toLongLong());
}

bool copy_dir_recursive(QString from_dir, QString to_dir)
{
    QDir dir;
    dir.setPath(from_dir);

    from_dir += QDir::separator();
    to_dir += QDir::separator();

    const auto fileEntries = dir.entryList(QDir::Files);
    const auto dirEntries = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    for (const auto &copy_file : fileEntries) {
        QString from = from_dir + copy_file;
        QString to = to_dir + copy_file;

        if (QFile::copy(from, to) == false) {
            return false;
        }
    }

    for (const auto &copy_dir : dirEntries) {
        QString from = from_dir + copy_dir;
        QString to = to_dir + copy_dir;

        if (dir.mkpath(to) == false) {
            return false;
        }

        if (copy_dir_recursive(from, to) == false) {
            return false;
        }
    }

    return true;
}

ConfigFile::ConfigFile()
{
    // QDesktopServices uses the application name to create a config path
    qApp->setApplicationName(Theme::instance()->appNameGUI());

    QSettings::setDefaultFormat(QSettings::IniFormat);

    const QString config = configFile();

    QSettings settings(config, QSettings::IniFormat);
    settings.beginGroup(defaultConnectionGroupName());
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

bool ConfigFile::optionalServerNotifications() const
{
    QSettings settings(configFile(), QSettings::IniFormat);
    return settings.value(optionalServerNotificationsC, true).toBool();
}

bool ConfigFile::showChatNotifications() const
{
    const QSettings settings(configFile(), QSettings::IniFormat);
    return settings.value(showChatNotificationsC, true).toBool();
}

void ConfigFile::setShowChatNotifications(const bool show)
{
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setValue(showChatNotificationsC, show);
    settings.sync();
}

bool ConfigFile::showCallNotifications() const
{
    const QSettings settings(configFile(), QSettings::IniFormat);
    return settings.value(showCallNotificationsC, true).toBool();
}

void ConfigFile::setShowCallNotifications(bool show)
{
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setValue(showCallNotificationsC, show);
    settings.sync();
}

bool ConfigFile::showQuotaWarningNotifications() const
{
    const QSettings settings(configFile(), QSettings::IniFormat);
    return settings.value(showQuotaWarningNotificationsC, true).toBool();
}

void ConfigFile::setShowQuotaWarningNotifications(bool show)
{
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setValue(showQuotaWarningNotificationsC, show);
    settings.sync();
}

bool ConfigFile::showInExplorerNavigationPane() const
{
    const bool defaultValue =
#ifdef Q_OS_WIN
        QOperatingSystemVersion::current() >= QOperatingSystemVersion::Windows10;
#else
        false
#endif
        ;
    QSettings settings(configFile(), QSettings::IniFormat);
    return settings.value(showInExplorerNavigationPaneC, defaultValue).toBool();
}

void ConfigFile::setShowInExplorerNavigationPane(bool show)
{
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setValue(showInExplorerNavigationPaneC, show);
    settings.sync();
}

int ConfigFile::timeout() const
{
    QSettings settings(configFile(), QSettings::IniFormat);
    return settings.value(QLatin1String(timeoutC), 300).toInt(); // default to 5 min
}

qint64 ConfigFile::chunkSize() const
{
    QSettings settings(configFile(), QSettings::IniFormat);
    return settings.value(QLatin1String(chunkSizeC), 100LL * 1024LL * 1024LL).toLongLong(); // 100MiB
}

qint64 ConfigFile::maxChunkSize() const
{
    QSettings settings(configFile(), QSettings::IniFormat);
    return settings.value(QLatin1String(maxChunkSizeC), 100LL * 1024LL * 1024LL).toLongLong(); // default to 100 MiB
}

qint64 ConfigFile::minChunkSize() const
{
    QSettings settings(configFile(), QSettings::IniFormat);
    return settings.value(QLatin1String(minChunkSizeC), 5LL * 1024LL * 1024LL).toLongLong(); // default to 5 MiB
}

chrono::milliseconds ConfigFile::targetChunkUploadDuration() const
{
    QSettings settings(configFile(), QSettings::IniFormat);
    return millisecondsValue(settings, targetChunkUploadDurationC, chrono::minutes(1));
}

void ConfigFile::setOptionalServerNotifications(bool show)
{
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setValue(optionalServerNotificationsC, show);
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
#else
    Q_UNUSED(w)
#endif
}

void ConfigFile::restoreGeometry(QWidget *w)
{
#ifndef TOKEN_AUTH_ONLY
    w->restoreGeometry(getValue(geometryC, w->objectName()).toByteArray());
#else
    Q_UNUSED(w)
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
#else
    Q_UNUSED(header)
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
#else
    Q_UNUSED(header)
#endif
}

QVariant ConfigFile::getPolicySetting(const QString &setting, const QVariant &defaultValue) const
{
    if (Utility::isWindows()) {
        const auto appName = isUnbrandedToBrandedMigrationInProgress() ? unbrandedAppName : Theme::instance()->appNameGUI();
        // check for policies first and return immediately if a value is found.
        QSettings userPolicy(QString::fromLatin1(R"(HKEY_CURRENT_USER\Software\Policies\%1\%2)").arg(APPLICATION_VENDOR, appName),
            QSettings::NativeFormat);
        if (userPolicy.contains(setting)) {
            return userPolicy.value(setting);
        }

        QSettings machinePolicy(QString::fromLatin1(R"(HKEY_LOCAL_MACHINE\Software\Policies\%1\%2)").arg(APPLICATION_VENDOR, appName),
            QSettings::NativeFormat);
        if (machinePolicy.contains(setting)) {
            return machinePolicy.value(setting);
        }
    }
    return defaultValue;
}

QString ConfigFile::configPath() const
{
    if (_confDir.isEmpty()) {
        if (!Utility::isWindows()) {
            // On Unix, use the AppConfigLocation for the settings, that's configurable with the XDG_CONFIG_HOME env variable.
            _confDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
        } else {
            // On Windows, use AppDataLocation, that's where the roaming data is and where we should store the config file
             auto newLocation = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

             // Check if this is the first time loading the new location
             if (!QFileInfo(newLocation).isDir()) {
                 // Migrate data to the new locations
                 auto oldLocation = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);

                 // Only migrate if the old location exists.
                 if (QFileInfo(oldLocation).isDir()) {
                     QDir().mkpath(newLocation);
                     copy_dir_recursive(oldLocation, newLocation);
                 }
             }
            _confDir = newLocation;
        }
    }

    return Utility::trailingSlashPath(_confDir);
}

static const QLatin1String syncExclFile("sync-exclude.lst");
static const QLatin1String exclFile("exclude.lst");

QString ConfigFile::excludeFile(Scope scope) const
{
    if (scope == SystemScope) {
        return ConfigFile::excludeFileFromSystem();
    }

    const auto excludeFilePath = scope == LegacyScope ? discoveredLegacyConfigPath() : configPath();

    // prefer sync-exclude.lst, but if it does not exist, check for exclude.lst
    QFileInfo exclFileInfo(excludeFilePath, syncExclFile);
    if (!exclFileInfo.isReadable()) {
        exclFileInfo.setFile(excludeFilePath, exclFile);
    }
    if (!exclFileInfo.isReadable()) {
        exclFileInfo.setFile(excludeFilePath, syncExclFile);
    }

    return exclFileInfo.absoluteFilePath();
}

QString ConfigFile::excludeFileFromSystem()
{
    QFileInfo fi;
#ifdef Q_OS_WIN
    fi.setFile(QCoreApplication::applicationDirPath(), syncExclFile);
#endif
#ifdef Q_OS_UNIX
    fi.setFile(QString(SYSCONFDIR "/" + Theme::instance()->appName()), syncExclFile);
    if (!fi.exists()) {
        // Prefer to return the preferred path! Only use the fallback location
        // if the other path does not exist and the fallback is valid.
        QFileInfo nextToBinary(QCoreApplication::applicationDirPath(), syncExclFile);
        if (nextToBinary.exists()) {
            fi = nextToBinary;
        } else {
            // For AppImage, the file might reside under a temporary mount path
            QDir d(QCoreApplication::applicationDirPath()); // supposed to be /tmp/mount.xyz/usr/bin
            d.cdUp(); // go out of bin
            d.cdUp(); // go out of usr
            if (!d.isRoot()) { // it is really a mountpoint
                if (d.cd("etc") && d.cd(Theme::instance()->appName())) {
                    QFileInfo inMountDir(d, syncExclFile);
                    if (inMountDir.exists()) {
                        fi = inMountDir;
                    }
                };
            }
        }
    }
#endif
#ifdef Q_OS_MACOS
    // exec path is inside the bundle
    fi.setFile(QCoreApplication::applicationDirPath(),
        QLatin1String("../Resources/") + syncExclFile);
#endif

    return fi.absoluteFilePath();
}

void OCC::ConfigFile::cleanUpdaterConfiguration()
{
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.beginGroup("Updater");
    settings.remove("autoUpdateAttempted");
    settings.remove("updateTargetVersion");
    settings.remove("updateTargetVersionString");
    settings.remove("updateAvailable");
    settings.sync();
}

void OCC::ConfigFile::cleanupGlobalNetworkConfiguration()
{
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.remove(useUploadLimitC);
    settings.remove(useDownloadLimitC);
    settings.remove(uploadLimitC);
    settings.remove(downloadLimitC);
    settings.sync();
}

QString ConfigFile::backup(const QString &fileName) const
{
    const QString baseFilePath = configPath() + fileName;
    auto versionString = clientVersionString();

    if (!versionString.isEmpty()) {
        versionString.prepend('_');
    }

    QString backupFile =
        QStringLiteral("%1.backup_%2%3")
            .arg(baseFilePath)
            .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"))
            .arg(versionString);

    // If this exact file already exists it's most likely that a backup was
    // already done. (two backup calls directly after each other, potentially
    // even with source alterations in between!)
    // QFile does not overwrite backupFile
    if(!QFile::copy(baseFilePath, backupFile)) {
        qCWarning(lcConfigFile) << "Failed to create a backup of the config file" << baseFilePath;
    }

    return backupFile;
}

bool ConfigFile::showConfigBackupWarning() const
{
    return getValue(showConfigBackupWarningC, QString(), false).toBool();
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

QString ConfigFile::defaultConnectionGroupName() const
{
    return Theme::instance()->appName();
}

void ConfigFile::storeData(const QString &group, const QString &key, const QVariant &value)
{
    const QString groupName(group.isEmpty() ? defaultConnectionGroupName() : group);
    QSettings settings(configFile(), QSettings::IniFormat);

    settings.beginGroup(groupName);
    settings.setValue(key, value);
    settings.sync();
}

QVariant ConfigFile::retrieveData(const QString &group, const QString &key) const
{
    const QString groupName(group.isEmpty() ? defaultConnectionGroupName() : group);
    QSettings settings(configFile(), QSettings::IniFormat);

    settings.beginGroup(groupName);
    return settings.value(key);
}

void ConfigFile::removeData(const QString &group, const QString &key)
{
    const QString groupName(group.isEmpty() ? defaultConnectionGroupName() : group);
    QSettings settings(configFile(), QSettings::IniFormat);

    settings.beginGroup(groupName);
    settings.remove(key);
}

bool ConfigFile::dataExists(const QString &group, const QString &key) const
{
    const QString groupName(group.isEmpty() ? defaultConnectionGroupName() : group);
    QSettings settings(configFile(), QSettings::IniFormat);

    settings.beginGroup(groupName);
    return settings.contains(key);
}

chrono::milliseconds ConfigFile::remotePollInterval(const QString &connectionGroupName) const
{
    const auto groupName = connectionGroupName.isEmpty() ? defaultConnectionGroupName() : connectionGroupName;
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.beginGroup(groupName);

    auto defaultPollInterval = chrono::milliseconds(DEFAULT_REMOTE_POLL_INTERVAL);
    auto remoteInterval = millisecondsValue(settings, remotePollIntervalC, defaultPollInterval);
    if (remoteInterval < chrono::seconds(5)) {
        qCWarning(lcConfigFile) << "Remote Interval is less than 5 seconds, reverting to" << DEFAULT_REMOTE_POLL_INTERVAL;
        remoteInterval = defaultPollInterval;
    }
    return remoteInterval;
}

void ConfigFile::setRemotePollInterval(chrono::milliseconds interval, const QString &connectionGroupName)
{
    const auto groupName = connectionGroupName.isEmpty() ? defaultConnectionGroupName() : connectionGroupName;
    if (interval < chrono::seconds(5)) {
        qCWarning(lcConfigFile) << "Remote Poll interval of " << interval.count() << " is below five seconds.";
        return;
    }
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.beginGroup(groupName);
    settings.setValue(QLatin1String(remotePollIntervalC), qlonglong(interval.count()));
    settings.sync();
}

chrono::milliseconds ConfigFile::forceSyncInterval(const QString &connectionGroupName) const
{
    const auto groupName = connectionGroupName.isEmpty() ? defaultConnectionGroupName() : connectionGroupName;
    auto pollInterval = remotePollInterval(groupName);
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.beginGroup(groupName);
    auto defaultInterval = chrono::hours(2);
    auto interval = millisecondsValue(settings, forceSyncIntervalC, defaultInterval);
    if (interval < pollInterval) {
        qCWarning(lcConfigFile) << "Force sync interval is less than the remote poll interval, reverting to" << pollInterval.count();
        interval = pollInterval;
    }
    return interval;
}

chrono::milliseconds OCC::ConfigFile::fullLocalDiscoveryInterval() const
{
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.beginGroup(defaultConnectionGroupName());
    return millisecondsValue(settings, fullLocalDiscoveryIntervalC, chrono::hours(1));
}

chrono::milliseconds ConfigFile::notificationRefreshInterval(const QString &connectionGroupName) const
{
    const auto groupName = connectionGroupName.isEmpty() ? defaultConnectionGroupName() : connectionGroupName;
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.beginGroup(groupName);

    const auto defaultInterval = chrono::minutes(1);
    auto interval = millisecondsValue(settings, notificationRefreshIntervalC, defaultInterval);
    if (interval < chrono::minutes(1)) {
        qCWarning(lcConfigFile) << "Notification refresh interval smaller than one minute, setting to one minute";
        interval = chrono::minutes(1);
    }
    return interval;
}

chrono::milliseconds ConfigFile::updateCheckInterval(const QString &connectionGroupName) const
{
    const auto groupName = connectionGroupName.isEmpty() ? defaultConnectionGroupName() : connectionGroupName;
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.beginGroup(groupName);

    auto defaultInterval = chrono::hours(10);
    auto interval = millisecondsValue(settings, updateCheckIntervalC, defaultInterval);

    auto minInterval = chrono::minutes(5);
    if (interval < minInterval) {
        qCWarning(lcConfigFile) << "Update check interval less than five minutes, resetting to 5 minutes";
        interval = minInterval;
    }
    return interval;
}

bool ConfigFile::skipUpdateCheck(const QString &connectionGroupName) const
{
    const auto groupName = connectionGroupName.isEmpty() ? defaultConnectionGroupName() : connectionGroupName;
    QVariant fallback = getValue(QLatin1String(skipUpdateCheckC), groupName, false);
    fallback = getValue(QLatin1String(skipUpdateCheckC), QString(), fallback);

    QVariant value = getPolicySetting(QLatin1String(skipUpdateCheckC), fallback);
    return value.toBool();
}

void ConfigFile::setSkipUpdateCheck(bool skip, const QString &connectionGroupName)
{
    const auto groupName = connectionGroupName.isEmpty() ? defaultConnectionGroupName() : connectionGroupName;
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.beginGroup(groupName);

    settings.setValue(QLatin1String(skipUpdateCheckC), QVariant(skip));
    settings.sync();
}

bool ConfigFile::autoUpdateCheck(const QString &connectionGroupName) const
{
    const auto groupName = connectionGroupName.isEmpty() ? defaultConnectionGroupName() : connectionGroupName;
    QVariant fallback = getValue(QLatin1String(autoUpdateCheckC), groupName, true);
    fallback = getValue(QLatin1String(autoUpdateCheckC), QString(), fallback);

    QVariant value = getPolicySetting(QLatin1String(autoUpdateCheckC), fallback);
    return value.toBool();
}

void ConfigFile::setAutoUpdateCheck(bool autoCheck, const QString &connectionGroupName)
{
    const auto groupName = connectionGroupName.isEmpty() ? defaultConnectionGroupName() : connectionGroupName;
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.beginGroup(groupName);

    settings.setValue(QLatin1String(autoUpdateCheckC), QVariant(autoCheck));
    settings.sync();
}

int ConfigFile::updateSegment() const
{
    QSettings settings(configFile(), QSettings::IniFormat);
    int segment = settings.value(QLatin1String(updateSegmentC), -1).toInt();

    // Invalid? (Unset at the very first launch)
    if(segment < 0 || segment > 99) {
        // Save valid segment value, normally has to be done only once.
        segment = Utility::rand() % 99;
        settings.setValue(QLatin1String(updateSegmentC), segment);
    }

    return segment;
}

QStringList ConfigFile::validUpdateChannels() const
{
    const auto isBranded = Theme::instance()->isBranded();

    if (isBranded) {
        return {UpdateChannel::defaultUpdateChannel().toString()};
    }

    const QList<UpdateChannel> *channel_list = &UpdateChannel::defaultUpdateChannelList();
    if (serverHasValidSubscription()) {
        channel_list = &UpdateChannel::enterpriseUpdateChannelsList();
    }

    QStringList list;
    for (const auto &channel : *channel_list) {
        list.append(channel.toString());
    }

    return list;
}

QString ConfigFile::defaultUpdateChannel() const
{
    const auto isBranded = Theme::instance()->isBranded();
    if (serverHasValidSubscription() && !isBranded) {
        if (const auto serverChannel = desktopEnterpriseChannel();
            validUpdateChannels().contains(serverChannel)) {
            qCWarning(lcConfigFile()) << "Default update channel is" << serverChannel << "because that is the desktop enterprise channel returned by the server.";
            return serverChannel;
        }
    }

    if (const auto currentVersionSuffix = Theme::instance()->versionSuffix();
        validUpdateChannels().contains(currentVersionSuffix) && !isBranded) {
        qCWarning(lcConfigFile()) << "Default update channel is" << currentVersionSuffix << "because of the version suffix of the current client.";
        return currentVersionSuffix;
    }

    qCWarning(lcConfigFile()) << "Default update channel is" << UpdateChannel::defaultUpdateChannel().toString();
    return UpdateChannel::defaultUpdateChannel().toString();
}

QString ConfigFile::currentUpdateChannel() const
{
    if (const auto isBranded = Theme::instance()->isBranded(); isBranded) {
        return UpdateChannel::defaultUpdateChannel().toString();
    }

    QSettings settings(configFile(), QSettings::IniFormat);
    const auto currentChannel = UpdateChannel::fromString(settings.value(QLatin1String(updateChannelC), defaultUpdateChannel()).toString());
    if (serverHasValidSubscription()) {
        const auto enterpriseChannel = UpdateChannel::fromString(desktopEnterpriseChannel());
        return UpdateChannel::mostStable(currentChannel, enterpriseChannel).toString();
    }

    return currentChannel.toString();
}

void ConfigFile::setUpdateChannel(const QString &channel)
{
    if (!validUpdateChannels().contains(channel)) {
        qCWarning(lcConfigFile()) << "Received invalid update channel:"
                                  << channel
                                  << "can only accept" << validUpdateChannels() << ". Ignoring.";
        return;
    }

    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setValue(QLatin1String(updateChannelC), channel);
}

[[nodiscard]] QString ConfigFile::overrideServerUrl() const
{
    QSettings settings(configFile(), QSettings::IniFormat);
    return settings.value(QLatin1String(overrideServerUrlC), {}).toString();
}

void ConfigFile::setOverrideServerUrl(const QString &url)
{
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setValue(QLatin1String(overrideServerUrlC), url);
}

[[nodiscard]] QString ConfigFile::overrideLocalDir() const
{
    QSettings settings(configFile(), QSettings::IniFormat);
    return settings.value(QLatin1String(overrideLocalDirC), {}).toString();
}

void ConfigFile::setOverrideLocalDir(const QString &localDir)
{
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setValue(QLatin1String(overrideLocalDirC), localDir);
}

bool ConfigFile::isVfsEnabled() const
{
    QSettings settings(configFile(), QSettings::IniFormat);
    return settings.value({isVfsEnabledC}, {}).toBool();
}

void ConfigFile::setVfsEnabled(bool enabled)
{
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setValue({isVfsEnabledC}, enabled);
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

        if (pass.isEmpty()) {
            // Security: Don't keep password in config file
            settings.remove(QLatin1String(proxyPassC));

            // Delete password from keychain
            auto job = new KeychainChunk::DeleteJob(keychainProxyPasswordKey());
            job->exec();
        } else {
            // Write password to keychain
            auto job = new KeychainChunk::WriteJob(keychainProxyPasswordKey(), pass.toUtf8());
            if (job->exec()) {
                // Security: Don't keep password in config file
                settings.remove(QLatin1String(proxyPassC));
            }
        }
    }
    settings.sync();
}

QVariant ConfigFile::getValue(const QString &param, const QString &group,
    const QVariant &defaultValue) const
{
    QVariant systemSetting;
    const auto appName = isUnbrandedToBrandedMigrationInProgress() ? unbrandedAppName : Theme::instance()->appNameGUI();
    if (Utility::isMac()) {
        QSettings systemSettings(QLatin1String("/Library/Preferences/" APPLICATION_REV_DOMAIN ".plist"), QSettings::NativeFormat);
        if (!group.isEmpty()) {
            systemSettings.beginGroup(group);
        }
        systemSetting = systemSettings.value(param, defaultValue);
    } else if (Utility::isUnix()) {
        QSettings systemSettings(QString(SYSCONFDIR "/%1/%1.conf").arg(appName), QSettings::NativeFormat);
        if (!group.isEmpty()) {
            systemSettings.beginGroup(group);
        }
        systemSetting = systemSettings.value(param, defaultValue);
    } else { // Windows
        QSettings systemSettings(QString::fromLatin1(R"(HKEY_LOCAL_MACHINE\Software\%1\%2)").arg(APPLICATION_VENDOR, appName),
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
    QByteArray passEncoded = getValue(proxyPassC).toByteArray();
    auto pass = QString::fromUtf8(QByteArray::fromBase64(passEncoded));
    passEncoded.clear();

    const auto key = keychainProxyPasswordKey();

    if (!pass.isEmpty()) {
        // Security: Migrate password from config file to keychain
        auto job = new KeychainChunk::WriteJob(key, pass.toUtf8());
        if (job->exec()) {
            QSettings settings(configFile(), QSettings::IniFormat);
            settings.remove(QLatin1String(proxyPassC));
            qCInfo(lcConfigFile()) << "Migrated proxy password to keychain";
        }
    } else {
        // Read password from keychain
        auto job = new KeychainChunk::ReadJob(key);
        if (job->exec()) {
            pass = job->textData();
        }
    }

    return pass;
}

QString ConfigFile::keychainProxyPasswordKey() const
{
    return QString::fromLatin1("proxy-password");
}

int ConfigFile::useUploadLimit() const
{
    return getValue(useUploadLimitC, QString(), 0).toInt();
}

int ConfigFile::useDownloadLimit() const
{
    return getValue(useDownloadLimitC, QString(), 0).toInt();
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
    return getValue(uploadLimitC, QString(), 10).toInt();
}

int ConfigFile::downloadLimit() const
{
    return getValue(downloadLimitC, QString(), 80).toInt();
}

void ConfigFile::setUploadLimit(int kbytes)
{
    setValue(uploadLimitC, kbytes);
}

void ConfigFile::setDownloadLimit(int kbytes)
{
    setValue(downloadLimitC, kbytes);
}

QPair<bool, qint64> ConfigFile::newBigFolderSizeLimit() const
{
    auto defaultValue = Theme::instance()->newBigFolderSizeLimit();
    const auto fallback = getValue(newBigFolderSizeLimitC, QString(), defaultValue).toLongLong();
    const auto value = getPolicySetting(QLatin1String(newBigFolderSizeLimitC), fallback).toLongLong();
    const bool use = value >= 0 && useNewBigFolderSizeLimit();
    return qMakePair(use, qMax<qint64>(0, value));
}

void ConfigFile::setNewBigFolderSizeLimit(bool isChecked, qint64 mbytes)
{
    setValue(newBigFolderSizeLimitC, mbytes);
    setValue(useNewBigFolderSizeLimitC, isChecked);
}

bool ConfigFile::confirmExternalStorage() const
{
    const auto fallback = getValue(confirmExternalStorageC, QString(), true);
    return getPolicySetting(QLatin1String(confirmExternalStorageC), fallback).toBool();
}

bool ConfigFile::useNewBigFolderSizeLimit() const
{
    const auto fallback = getValue(useNewBigFolderSizeLimitC, QString(), true);
    return getPolicySetting(QLatin1String(useNewBigFolderSizeLimitC), fallback).toBool();
}

bool ConfigFile::notifyExistingFoldersOverLimit() const
{
    const auto fallback = getValue(notifyExistingFoldersOverLimitC, {}, false);
    return getPolicySetting(QString(notifyExistingFoldersOverLimitC), fallback).toBool();
}

void ConfigFile::setNotifyExistingFoldersOverLimit(const bool notify)
{
    setValue(notifyExistingFoldersOverLimitC, notify);
}

bool ConfigFile::stopSyncingExistingFoldersOverLimit() const
{
    const auto notifyExistingBigEnabled = notifyExistingFoldersOverLimit();
    const auto fallback = getValue(stopSyncingExistingFoldersOverLimitC, {}, notifyExistingBigEnabled);
    return getPolicySetting(QString(stopSyncingExistingFoldersOverLimitC), fallback).toBool();
}

void ConfigFile::setStopSyncingExistingFoldersOverLimit(const bool stopSyncing)
{
    setValue(stopSyncingExistingFoldersOverLimitC, stopSyncing);
}

void ConfigFile::setConfirmExternalStorage(bool isChecked)
{
    setValue(confirmExternalStorageC, isChecked);
}

bool ConfigFile::moveToTrash() const
{
    return getValue(moveToTrashC, QString(), false).toBool();
}

void ConfigFile::setMoveToTrash(bool isChecked)
{
    setValue(moveToTrashC, isChecked);
}

bool ConfigFile::forceLoginV2() const
{
    return getValue(forceLoginV2C, QString(), false).toBool();
}

void ConfigFile::setForceLoginV2(bool isChecked)
{
    setValue(forceLoginV2C, isChecked);
}

bool ConfigFile::showMainDialogAsNormalWindow() const {
    return getValue(showMainDialogAsNormalWindowC, {}, false).toBool();
}

bool ConfigFile::promptDeleteFiles() const
{
    QSettings settings(configFile(), QSettings::IniFormat);
    return settings.value(promptDeleteC, false).toBool();
}

void ConfigFile::setPromptDeleteFiles(bool promptDeleteFiles)
{
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setValue(promptDeleteC, promptDeleteFiles);
}

int ConfigFile::deleteFilesThreshold() const
{
    QSettings settings(configFile(), QSettings::IniFormat);
    return settings.value(QLatin1String(deleteFilesThresholdC), deleteFilesThresholdDefaultValue).toInt();
}

void ConfigFile::setDeleteFilesThreshold(int thresholdValue)
{
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setValue(QLatin1String(deleteFilesThresholdC), thresholdValue);
}

bool ConfigFile::monoIcons() const
{
    QSettings settings(configFile(), QSettings::IniFormat);
    bool monoDefault = false; // On Mac we want bw by default
#ifdef Q_OS_MACOS
    // OEM themes are not obliged to ship mono icons
    monoDefault = QByteArrayLiteral("Nextcloud") == QByteArrayLiteral(APPLICATION_NAME);
#endif
    return settings.value(QLatin1String(monoIconsC), monoDefault).toBool();
}

void ConfigFile::setMonoIcons(bool useMonoIcons)
{
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setValue(QLatin1String(monoIconsC), useMonoIcons);
}

bool ConfigFile::automaticLogDir() const
{
    QSettings settings(configFile(), QSettings::IniFormat);
    return settings.value(QLatin1String(automaticLogDirC), false).toBool();
}

void ConfigFile::setAutomaticLogDir(bool enabled)
{
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setValue(QLatin1String(automaticLogDirC), enabled);
}

QString ConfigFile::logDir() const
{
    const auto defaultLogDir = QString(configPath() + QStringLiteral("/logs"));
    QSettings settings(configFile(), QSettings::IniFormat);
    return settings.value(QLatin1String(logDirC), defaultLogDir).toString();
}

void ConfigFile::setLogDir(const QString &dir)
{
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setValue(QLatin1String(logDirC), dir);
}

bool ConfigFile::logDebug() const
{
    QSettings settings(configFile(), QSettings::IniFormat);
    return settings.value(QLatin1String(logDebugC), false).toBool();
}

void ConfigFile::setLogDebug(bool enabled)
{
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setValue(QLatin1String(logDebugC), enabled);
}

int ConfigFile::logExpire() const
{
    QSettings settings(configFile(), QSettings::IniFormat);
    return settings.value(QLatin1String(logExpireC), 24).toInt();
}

void ConfigFile::setLogExpire(int hours)
{
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setValue(QLatin1String(logExpireC), hours);
}

bool ConfigFile::logFlush() const
{
    QSettings settings(configFile(), QSettings::IniFormat);
    return settings.value(QLatin1String(logFlushC), false).toBool();
}

void ConfigFile::setLogFlush(bool enabled)
{
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setValue(QLatin1String(logFlushC), enabled);
}

bool ConfigFile::showExperimentalOptions() const
{
    QSettings settings(configFile(), QSettings::IniFormat);
    return settings.value(QLatin1String(showExperimentalOptionsC), false).toBool();
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

QString ConfigFile::clientVersionString() const
{
    QSettings settings(configFile(), QSettings::IniFormat);
    return settings.value(QLatin1String(clientVersionC), QString()).toString();
}

void ConfigFile::setClientVersionString(const QString &version)
{
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setValue(QLatin1String(clientVersionC), version);
}

QString ConfigFile::clientPreviousVersionString() const
{
    QSettings settings(configFile(), QSettings::IniFormat);
    return settings.value(QLatin1String(clientPreviousVersionC), QString()).toString();
}

void ConfigFile::setClientPreviousVersionString(const QString &version)
{
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setValue(QLatin1String(clientPreviousVersionC), version);
}

bool ConfigFile::launchOnSystemStartup() const
{
    QSettings settings(configFile(), QSettings::IniFormat);
    return settings.value(launchOnSystemStartupC, true).toBool();
}

void ConfigFile::setLaunchOnSystemStartup(const bool autostart)
{
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setValue(launchOnSystemStartupC, autostart);
}

bool ConfigFile::serverHasValidSubscription() const
{
    QSettings settings(configFile(), QSettings::IniFormat);
    return settings.value(QLatin1String(serverHasValidSubscriptionC), false).toBool();
}

void ConfigFile::setServerHasValidSubscription(const bool valid)
{
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setValue(QLatin1String(serverHasValidSubscriptionC), valid);
}

QString ConfigFile::desktopEnterpriseChannel() const
{
    QSettings settings(configFile(), QSettings::IniFormat);
    return settings.value(QLatin1String(desktopEnterpriseChannelName), UpdateChannel::defaultUpdateChannel().toString()).toString();
}

void ConfigFile::setDesktopEnterpriseChannel(const QString &channel)
{
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setValue(QLatin1String(desktopEnterpriseChannelName), UpdateChannel::fromString(channel).toString());
}

QString ConfigFile::language() const
{
    QSettings settings(configFile(), QSettings::IniFormat);
    return settings.value(QLatin1String(languageC), QLatin1String("")).toString();
}

void ConfigFile::setLanguage(const QString& language)
{
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setValue(QLatin1String(languageC), language);
}

uint ConfigFile::lastSelectedAccount() const
{
    QSettings settings(configFile(), QSettings::IniFormat);
    return settings.value(QLatin1String(lastSelectedAccountC), QLatin1String("")).toUInt();
}

void ConfigFile::setLastSelectedAccount(const uint accountId)
{
    QSettings settings(configFile(), QSettings::IniFormat);
    settings.setValue(QLatin1String(lastSelectedAccountC), accountId);
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

void ConfigFile::setupDefaultExcludeFilePaths(ExcludedFiles &excludedFiles)
{
    ConfigFile cfg;
    const auto systemList = cfg.excludeFile(ConfigFile::SystemScope);
    const auto userList = cfg.excludeFile(ConfigFile::UserScope);
    const auto legacyList = cfg.excludeFile(ConfigFile::LegacyScope);

    if (Theme::instance()->isBranded() && QFile::exists(systemList) && QFile::copy(systemList, userList)) {
        qCInfo(lcConfigFile) << "Overwriting user list" << userList << "with system list" << systemList;
        excludedFiles.addExcludeFilePath(systemList);
        return;
    }

    if (!QFile::exists(userList)) {
        qCInfo(lcConfigFile) << "User defined ignore list does not exist:" << userList;

        if (QFile::exists(legacyList) && QFile::copy(legacyList, userList)) {
            qCInfo(lcConfigFile) << "Migrating legacy list" << legacyList << "to user list" << userList;

        } else if (QFile::copy(systemList, userList)) {
            qCInfo(lcConfigFile) << "Migrating system list" << legacyList << "to user list" << userList;
        }
    }

    if (!QFile::exists(userList)) {
        qCInfo(lcConfigFile) << "Adding system ignore list to csync:" << systemList;
        excludedFiles.addExcludeFilePath(systemList);
        return;
    }

    qCInfo(lcConfigFile) << "Adding user defined ignore list to csync:" << userList;
    excludedFiles.addExcludeFilePath(userList);
}

QString ConfigFile::discoveredLegacyConfigPath()
{
    return _discoveredLegacyConfigPath;
}

void ConfigFile::setDiscoveredLegacyConfigPath(const QString &discoveredLegacyConfigPath)
{
    if (_discoveredLegacyConfigPath == discoveredLegacyConfigPath) {
        return;
    }

    _discoveredLegacyConfigPath = discoveredLegacyConfigPath;
}

QString ConfigFile::fileProviderDomainUuidFromAccountId(const QString &accountId) const
{
    if (accountId.isEmpty()) {
        return {};
    }
    return retrieveData(QStringLiteral("FileProviderDomainUuids"), accountId).toString();
}

void ConfigFile::setFileProviderDomainUuidForAccountId(const QString &accountId, const QString &domainUuid)
{
    if (accountId.isEmpty() || domainUuid.isEmpty()) {
        return;
    }

    storeData(QStringLiteral("FileProviderDomainUuids"), accountId, domainUuid);
    storeData(QStringLiteral("FileProviderAccountIds"), domainUuid, accountId);
}

QString ConfigFile::accountIdFromFileProviderDomainUuid(const QString &domainUuid) const
{
    if (domainUuid.isEmpty()) {
        return {};
    }

    return retrieveData(QStringLiteral("FileProviderAccountIds"), domainUuid).toString();
}

void ConfigFile::removeFileProviderDomainUuidMapping(const QString &accountId)
{
    if (accountId.isEmpty()) {
        return;
    }

    const QString domainUuid = fileProviderDomainUuidFromAccountId(accountId);

    if (!domainUuid.isEmpty()) {
        removeData(QStringLiteral("FileProviderAccountIds"), domainUuid);
    }

    removeData(QStringLiteral("FileProviderDomainUuids"), accountId);
}

void ConfigFile::removeFileProviderDomainMappingByDomainIdentifier(const QString domainIdentifier)
{
    if (domainIdentifier.isEmpty()) {
        return;
    }

    removeData(QStringLiteral("FileProviderAccountIds"), domainIdentifier);

    const QString accountIdentifier = accountIdFromFileProviderDomainUuid(domainIdentifier);

    if (!accountIdentifier.isEmpty()) {
        removeData(QStringLiteral("FileProviderDomainUuids"), accountIdentifier);
    }
}

bool ConfigFile::isUpgrade() const
{
    const auto currentVersion = QVersionNumber::fromString(MIRALL_VERSION_STRING);
    const auto previousVersion = QVersionNumber::fromString(clientPreviousVersionString());
    return currentVersion > previousVersion;
}

bool ConfigFile::isDowngrade() const
{
    const auto currentVersion = QVersionNumber::fromString(MIRALL_VERSION_STRING);
    const auto previousVersion = QVersionNumber::fromString(clientPreviousVersionString());
    return previousVersion > currentVersion;
}

bool ConfigFile::shouldTryUnbrandedToBrandedMigration() const
{
    return migrationPhase() == ConfigFile::MigrationPhase::SetupFolders
        && Theme::instance()->appName() != unbrandedAppName;
}

bool ConfigFile::isUnbrandedToBrandedMigrationInProgress() const
{
    return isMigrationInProgress() && Theme::instance()->appName() != unbrandedAppName;
}

bool ConfigFile::shouldTryToMigrate() const
{
    return !isClientVersionSet() && (isUpgrade() || isDowngrade());
}

bool ConfigFile::isClientVersionSet() const
{
    const auto currentVersion = QVersionNumber::fromString(MIRALL_VERSION_STRING);
    const auto clientConfigVersion = QVersionNumber::fromString(clientVersionString());
    const auto isVersionSet = !clientConfigVersion.isNull() && !clientPreviousVersionString().isEmpty();
    return isVersionSet && clientConfigVersion == currentVersion;
}

bool ConfigFile::isMigrationInProgress() const
{
    return _migrationPhase != MigrationPhase::NotStarted && _migrationPhase != MigrationPhase::Done;
}

void ConfigFile::setMigrationPhase(const MigrationPhase phase)
{
    // do not rollback
    if (phase > _migrationPhase) {
        _migrationPhase = phase;
    }
}

ConfigFile::MigrationPhase ConfigFile::migrationPhase() const
{
    return _migrationPhase;
}

}
