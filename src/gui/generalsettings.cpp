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

#include "generalsettings.h"
#include "ui_generalsettings.h"

#include "theme.h"
#include "configfile.h"
#include "application.h"
#include "owncloudsetupwizard.h"
#include "accountmanager.h"
#include "guiutility.h"

#if defined(BUILD_UPDATER)
#include "updater/updater.h"
#include "updater/ocupdater.h"
#ifdef Q_OS_MAC
// FIXME We should unify those, but Sparkle does everything behind the scene transparently
#include "updater/sparkleupdater.h"
#endif
#endif

#ifdef BUILD_FILE_PROVIDER_MODULE
#include "macOS/fileprovider.h"
#include "macOS/fileprovidersettingscontroller.h"
#endif

#include "ignorelisteditor.h"
#include "common/utility.h"
#include "logger.h"

#include "legalnotice.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QNetworkProxy>
#include <QDir>
#include <QScopedValueRollback>
#include <QMessageBox>

#include <KZip>

namespace {
struct ZipEntry {
    QString localFilename;
    QString zipFilename;
};

ZipEntry fileInfoToZipEntry(const QFileInfo &info)
{
    return {
        info.absoluteFilePath(),
        info.fileName()
    };
}

ZipEntry fileInfoToLogZipEntry(const QFileInfo &info)
{
    auto entry = fileInfoToZipEntry(info);
    entry.zipFilename.prepend(QStringLiteral("logs/"));
    return entry;
}

QVector<ZipEntry> syncFolderToDatabaseZipEntry(OCC::Folder *f)
{
    QVector<ZipEntry> result;

    const auto journalPath = f->journalDb()->databaseFilePath();
    const auto journalInfo = QFileInfo(journalPath);
    const auto walJournalInfo = QFileInfo(journalPath + "-wal");
    const auto shmJournalInfo = QFileInfo(journalPath + "-shm");

    result += fileInfoToZipEntry(journalInfo);
    if (walJournalInfo.exists()) {
        result += fileInfoToZipEntry(walJournalInfo);
    }
    if (shmJournalInfo.exists()) {
        result += fileInfoToZipEntry(shmJournalInfo);
    }

    return result;
}

QVector<ZipEntry> createDebugArchiveFileList()
{
    auto list = QVector<ZipEntry>();
    OCC::ConfigFile cfg;

    list.append(fileInfoToZipEntry(QFileInfo(cfg.configFile())));

    const auto logger = OCC::Logger::instance();

    if (!logger->logDir().isEmpty()) {
        QDir dir(logger->logDir());
        const auto infoList = dir.entryInfoList(QDir::Files);
        std::transform(std::cbegin(infoList), std::cend(infoList),
                       std::back_inserter(list),
                       fileInfoToLogZipEntry);
    } else if (!logger->logFile().isEmpty()) {
        list.append(fileInfoToZipEntry(QFileInfo(logger->logFile())));
    }

    const auto folders = OCC::FolderMan::instance()->map().values();
    std::for_each(std::cbegin(folders), std::cend(folders),
                  [&list] (auto &folderIt) {
                      const auto &newEntries = syncFolderToDatabaseZipEntry(folderIt);
                      std::copy(std::cbegin(newEntries), std::cend(newEntries), std::back_inserter(list));
                  });

    return list;
}

bool createDebugArchive(const QString &filename)
{
    const auto fileInfo = QFileInfo(filename);
    const auto dirInfo = QFileInfo(fileInfo.dir().absolutePath());
    if (!dirInfo.isWritable()) {
        QMessageBox::critical(
            nullptr,
            QObject::tr("Failed to create debug archive"),
            QObject::tr("Could not create debug archive in selected location!"),
            QMessageBox::Ok
        );
        return false;
    }

    const auto entries = createDebugArchiveFileList();

    KZip zip(filename);
    zip.open(QIODevice::WriteOnly);

    for (const auto &entry : entries) {
        zip.addLocalFile(entry.localFilename, entry.zipFilename);
    }

#ifdef BUILD_FILE_PROVIDER_MODULE
    const auto fileProvider = OCC::Mac::FileProvider::instance();
    if (fileProvider && fileProvider->fileProviderAvailable()) {
        const auto tempDir = QTemporaryDir();
        const auto xpc = fileProvider->xpc();
        const auto vfsAccounts = OCC::Mac::FileProviderSettingsController::instance()->vfsEnabledAccounts();
        for (const auto &accountUserIdAtHost : vfsAccounts) {
            const auto accountState = OCC::AccountManager::instance()->accountFromUserId(accountUserIdAtHost);
            if (!accountState) {
                qWarning() << "Could not find account for" << accountUserIdAtHost;
                continue;
            }
            const auto account = accountState->account();
            const auto vfsLogFilename = QStringLiteral("macOS_vfs_%1.log").arg(account->davUser());
            const auto vfsLogPath = tempDir.filePath(vfsLogFilename);
            xpc->createDebugArchiveForExtension(accountUserIdAtHost, vfsLogPath);
            zip.addLocalFile(vfsLogPath, vfsLogFilename);
        }
    }
#endif

    const auto clientParameters = QCoreApplication::arguments().join(' ').toUtf8();
    zip.prepareWriting("__nextcloud_client_parameters.txt", {}, {}, clientParameters.size());
    zip.writeData(clientParameters, clientParameters.size());
    zip.finishWriting(clientParameters.size());

    const auto buildInfo = QString(OCC::Theme::instance()->aboutInfo() + "\n\n" + OCC::Theme::instance()->aboutDetails()).toUtf8();
    zip.prepareWriting("__nextcloud_client_buildinfo.txt", {}, {}, buildInfo.size());
    zip.writeData(buildInfo, buildInfo.size());
    zip.finishWriting(buildInfo.size());
    return true;
}

}

namespace OCC {

GeneralSettings::GeneralSettings(QWidget *parent)
    : QWidget(parent)
    , _ui(new Ui::GeneralSettings)
{
    _ui->setupUi(this);

    connect(_ui->serverNotificationsCheckBox, &QAbstractButton::toggled,
        this, &GeneralSettings::slotToggleOptionalServerNotifications);
    _ui->serverNotificationsCheckBox->setToolTip(tr("Server notifications that require attention."));

    connect(_ui->chatNotificationsCheckBox, &QAbstractButton::toggled,
            this, &GeneralSettings::slotToggleChatNotifications);
    _ui->chatNotificationsCheckBox->setToolTip(tr("Show chat notification dialogs."));

    connect(_ui->callNotificationsCheckBox, &QAbstractButton::toggled,
        this, &GeneralSettings::slotToggleCallNotifications);
    _ui->callNotificationsCheckBox->setToolTip(tr("Show call notification dialogs."));

    connect(_ui->showInExplorerNavigationPaneCheckBox, &QAbstractButton::toggled, this, &GeneralSettings::slotShowInExplorerNavigationPane);

    // Rename 'Explorer' appropriately on non-Windows
#ifdef Q_OS_MAC
    QString txt = _ui->showInExplorerNavigationPaneCheckBox->text();
    txt.replace(QString::fromLatin1("Explorer"), QString::fromLatin1("Finder"));
    _ui->showInExplorerNavigationPaneCheckBox->setText(txt);
#endif

    if(const auto hasSystemAutoStart = Utility::hasSystemLaunchOnStartup(Theme::instance()->appName())) {
        _ui->autostartCheckBox->setChecked(hasSystemAutoStart);
        _ui->autostartCheckBox->setDisabled(hasSystemAutoStart);
        _ui->autostartCheckBox->setToolTip(tr("You cannot disable autostart because system-wide autostart is enabled."));
    } else {       
        connect(_ui->autostartCheckBox, &QAbstractButton::toggled, this, &GeneralSettings::slotToggleLaunchOnStartup);
        _ui->autostartCheckBox->setChecked(ConfigFile().launchOnSystemStartup());
    }

    // setup about section
    _ui->infoAndUpdatesLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextBrowserInteraction);
    _ui->infoAndUpdatesLabel->setText(Theme::instance()->about());
    _ui->infoAndUpdatesLabel->setOpenExternalLinks(true);

    // About legal notice
    connect(_ui->legalNoticeButton, &QPushButton::clicked, this, &GeneralSettings::slotShowLegalNotice);

    connect(_ui->usageDocumentationButton, &QPushButton::clicked, this, []() {
        Utility::openBrowser(QUrl(Theme::instance()->helpUrl()));
    });

    loadMiscSettings();

    // misc
    connect(_ui->monoIconsCheckBox, &QAbstractButton::toggled, this, &GeneralSettings::saveMiscSettings);
    connect(_ui->crashreporterCheckBox, &QAbstractButton::toggled, this, &GeneralSettings::saveMiscSettings);
    connect(_ui->newFolderLimitCheckBox, &QAbstractButton::toggled, this, &GeneralSettings::saveMiscSettings);
    connect(_ui->newFolderLimitSpinBox, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &GeneralSettings::saveMiscSettings);
    connect(_ui->existingFolderLimitCheckBox, &QAbstractButton::toggled, this, &GeneralSettings::saveMiscSettings);
    connect(_ui->stopExistingFolderNowBigSyncCheckBox, &QAbstractButton::toggled, this, &GeneralSettings::saveMiscSettings);
    connect(_ui->newExternalStorage, &QAbstractButton::toggled, this, &GeneralSettings::saveMiscSettings);
    connect(_ui->moveFilesToTrashCheckBox, &QAbstractButton::toggled, this, &GeneralSettings::saveMiscSettings);

#ifndef WITH_CRASHREPORTER
    _ui->crashreporterCheckBox->setVisible(false);
#endif

    // Hide on non-Windows, or WindowsVersion < 10.
    // The condition should match the default value of ConfigFile::showInExplorerNavigationPane.
#ifdef Q_OS_WIN
        if (QOperatingSystemVersion::current() < QOperatingSystemVersion::Windows10)
            _ui->showInExplorerNavigationPaneCheckBox->setVisible(false);
#else
    // Hide on non-Windows
    _ui->showInExplorerNavigationPaneCheckBox->setVisible(false);
#endif

    /* Set the left contents margin of the layout to zero to make the checkboxes
     * align properly vertically , fixes bug #3758
     */
    int m0 = 0;
    int m1 = 0;
    int m2 = 0;
    int m3 = 0;
    _ui->horizontalLayout_3->getContentsMargins(&m0, &m1, &m2, &m3);
    _ui->horizontalLayout_3->setContentsMargins(0, m1, m2, m3);

    // OEM themes are not obliged to ship mono icons, so there
    // is no point in offering an option
    _ui->monoIconsCheckBox->setVisible(Theme::instance()->monoIconsAvailable());

    connect(_ui->ignoredFilesButton, &QAbstractButton::clicked, this, &GeneralSettings::slotIgnoreFilesEditor);
    connect(_ui->debugArchiveButton, &QAbstractButton::clicked, this, &GeneralSettings::slotCreateDebugArchive);

    // accountAdded means the wizard was finished and the wizard might change some options.
    connect(AccountManager::instance(), &AccountManager::accountAdded, this, &GeneralSettings::loadMiscSettings);

#if defined(BUILD_UPDATER)
    loadUpdateChannelsList();
#endif

    customizeStyle();
}

GeneralSettings::~GeneralSettings()
{
    delete _ui;
}

QSize GeneralSettings::sizeHint() const
{
    return {
        ownCloudGui::settingsDialogSize().width(),
        QWidget::sizeHint().height()
    };
}

void GeneralSettings::loadMiscSettings()
{
    QScopedValueRollback<bool> scope(_currentlyLoading, true);
    ConfigFile cfgFile;

    _ui->monoIconsCheckBox->setChecked(cfgFile.monoIcons());
    _ui->serverNotificationsCheckBox->setChecked(cfgFile.optionalServerNotifications());
    _ui->chatNotificationsCheckBox->setEnabled(cfgFile.optionalServerNotifications());
    _ui->chatNotificationsCheckBox->setChecked(cfgFile.showChatNotifications());
    _ui->callNotificationsCheckBox->setEnabled(cfgFile.optionalServerNotifications());
    _ui->callNotificationsCheckBox->setChecked(cfgFile.showCallNotifications());
    _ui->showInExplorerNavigationPaneCheckBox->setChecked(cfgFile.showInExplorerNavigationPane());
    _ui->crashreporterCheckBox->setChecked(cfgFile.crashReporter());
    _ui->newExternalStorage->setChecked(cfgFile.confirmExternalStorage());
    _ui->monoIconsCheckBox->setChecked(cfgFile.monoIcons());
    _ui->moveFilesToTrashCheckBox->setChecked(cfgFile.moveToTrash());

    auto newFolderLimit = cfgFile.newBigFolderSizeLimit();
    _ui->newFolderLimitCheckBox->setChecked(newFolderLimit.first);
    _ui->newFolderLimitSpinBox->setValue(newFolderLimit.second);
    _ui->existingFolderLimitCheckBox->setEnabled(_ui->newFolderLimitCheckBox->isChecked());
    _ui->existingFolderLimitCheckBox->setChecked(_ui->newFolderLimitCheckBox->isChecked() && cfgFile.notifyExistingFoldersOverLimit());
    _ui->stopExistingFolderNowBigSyncCheckBox->setEnabled(_ui->existingFolderLimitCheckBox->isChecked());
    _ui->stopExistingFolderNowBigSyncCheckBox->setChecked(_ui->existingFolderLimitCheckBox->isChecked() && cfgFile.stopSyncingExistingFoldersOverLimit());
    _ui->newExternalStorage->setChecked(cfgFile.confirmExternalStorage());
    _ui->monoIconsCheckBox->setChecked(cfgFile.monoIcons());
}

#if defined(BUILD_UPDATER)
void GeneralSettings::loadUpdateChannelsList() {
    ConfigFile cfgFile;
    const auto validUpdateChannels = cfgFile.validUpdateChannels();
    if (_currentUpdateChannelList.isEmpty() || (_currentUpdateChannelList != validUpdateChannels && !cfgFile.serverHasValidSubscription())) {
        _currentUpdateChannelList = validUpdateChannels;
        _ui->updateChannel->clear();
        _ui->updateChannel->addItems(_currentUpdateChannelList);
        const auto currentUpdateChannelIndex = _currentUpdateChannelList.indexOf(cfgFile.currentUpdateChannel());
        _ui->updateChannel->setCurrentIndex(currentUpdateChannelIndex != -1 ? currentUpdateChannelIndex : 0);
        connect(_ui->updateChannel, &QComboBox::currentTextChanged, this, &GeneralSettings::slotUpdateChannelChanged);
    }
}

void GeneralSettings::slotUpdateInfo()
{
    ConfigFile config;
    const auto updater = Updater::instance();
    if (config.skipUpdateCheck() || !updater) {
        // updater disabled on compile
        _ui->updatesContainer->setVisible(false);
        return;
    }

    if (updater) {
        connect(_ui->updateButton,
                &QAbstractButton::clicked,
                this,
                &GeneralSettings::slotUpdateCheckNow,
                Qt::UniqueConnection);
        connect(_ui->autoCheckForUpdatesCheckBox, &QAbstractButton::toggled, this,
                &GeneralSettings::slotToggleAutoUpdateCheck, Qt::UniqueConnection);
        _ui->autoCheckForUpdatesCheckBox->setChecked(config.autoUpdateCheck());
    }

    // Note: the sparkle-updater is not an OCUpdater
    const auto ocupdater = qobject_cast<OCUpdater *>(updater);
    if (ocupdater) {
        connect(ocupdater, &OCUpdater::downloadStateChanged, this, &GeneralSettings::slotUpdateInfo, Qt::UniqueConnection);
        connect(_ui->restartButton, &QAbstractButton::clicked, ocupdater, &OCUpdater::slotStartInstaller, Qt::UniqueConnection);

        auto status = ocupdater->statusString(OCUpdater::UpdateStatusStringFormat::Html);
        Theme::replaceLinkColorStringBackgroundAware(status);

        _ui->updateStateLabel->setOpenExternalLinks(false);
        connect(_ui->updateStateLabel, &QLabel::linkActivated, this, [](const QString &link) {
            Utility::openBrowser(QUrl(link));
        });
        _ui->updateStateLabel->setText(status);
        _ui->restartButton->setVisible(ocupdater->downloadState() == OCUpdater::DownloadComplete);
        _ui->updateButton->setEnabled(ocupdater->downloadState() != OCUpdater::CheckingServer &&
                                      ocupdater->downloadState() != OCUpdater::Downloading &&
                                      ocupdater->downloadState() != OCUpdater::DownloadComplete);
    }
#if defined(Q_OS_MAC) && defined(HAVE_SPARKLE)
    else if (const auto sparkleUpdater = qobject_cast<SparkleUpdater *>(updater)) {
        connect(sparkleUpdater, &SparkleUpdater::statusChanged, this, &GeneralSettings::slotUpdateInfo, Qt::UniqueConnection);
        _ui->updateStateLabel->setText(sparkleUpdater->statusString());
        _ui->restartButton->setVisible(false);

        const auto updaterState = sparkleUpdater->state();
        const auto enableUpdateButton = updaterState == SparkleUpdater::State::Idle ||
                                        updaterState == SparkleUpdater::State::Unknown;
        _ui->updateButton->setEnabled(enableUpdateButton);
    }
#endif
}

void GeneralSettings::slotUpdateChannelChanged()
{
    const auto updateChannelToLocalized = [](const QString &channel) {
        if (channel == QStringLiteral("stable")) {
            return tr("stable");
        }

        if (channel == QStringLiteral("beta")) {
            return tr("beta");
        }

        if (channel == QStringLiteral("daily")) {
            return tr("daily");
        }

        if (channel == QStringLiteral("enterprise")) {
            return tr("enterprise");
        }

        return QString{};
    };

    const auto updateChannelFromLocalized = [](const int index) {
        switch(index) {
        case 1:
            return QStringLiteral("beta");
            break;
        case 2:
            return QStringLiteral("daily");
            break;
        case 3:
            return QStringLiteral("enterprise");
            break;
        default:
            return QStringLiteral("stable");
        }
    };

    ConfigFile configFile;
    const auto channel = updateChannelFromLocalized(_ui->updateChannel->currentIndex());
    if (channel == configFile.currentUpdateChannel()) {
        return;
    }

    const auto nonEnterpriseOptions = tr("- beta: contains versions with new features that may not be tested thoroughly\n"
                                    "- daily: contains versions created daily only for testing and development\n"
                                    "\n"
                                    "Downgrading versions is not possible immediately: changing from beta to stable means waiting for the new stable version.",
                                    "list of available update channels to non enterprise users and downgrading warning");
    const auto enterpriseOptions = tr("- enterprise: contains stable versions for customers.\n"
                                    "\n"
                                    "Downgrading versions is not possible immediately: changing from stable to enterprise means waiting for the new enterprise version.",
                                    "list of available update channels to enterprise users and downgrading warning");

    auto msgBox = new QMessageBox(
        QMessageBox::Warning,
        tr("Changing update channel?"),
        tr("The channel determines which upgrades will be offered to install:\n"
           "- stable: contains tested versions considered reliable\n",
           "starts list of available update channels, stable is always available")
            .append(configFile.validUpdateChannels().contains("enterprise") ? enterpriseOptions : nonEnterpriseOptions),
        QMessageBox::NoButton,
        this);
    const auto acceptButton = msgBox->addButton(tr("Change update channel"), QMessageBox::AcceptRole);
    msgBox->addButton(tr("Cancel"), QMessageBox::RejectRole);
    connect(msgBox, &QMessageBox::finished, msgBox, [this, channel, msgBox, acceptButton, updateChannelToLocalized] {
        msgBox->deleteLater();
        if (msgBox->clickedButton() == acceptButton) {
            ConfigFile().setUpdateChannel(channel);
            if (auto updater = qobject_cast<OCUpdater *>(Updater::instance())) {
                updater->setUpdateUrl(Updater::updateUrl());
                updater->checkForUpdate();
            }
#if defined(Q_OS_MAC) && defined(HAVE_SPARKLE)
            else if (auto updater = qobject_cast<SparkleUpdater *>(Updater::instance())) {
                updater->setUpdateUrl(Updater::updateUrl());
                updater->checkForUpdate();
            }
#endif
        } else {
            _ui->updateChannel->setCurrentText(updateChannelToLocalized(ConfigFile().currentUpdateChannel()));
        }
    });
    msgBox->open();
}

void GeneralSettings::slotUpdateCheckNow()
{
#if defined(Q_OS_MAC) && defined(HAVE_SPARKLE)
    auto *updater = qobject_cast<SparkleUpdater *>(Updater::instance());
#else
    auto *updater = qobject_cast<OCUpdater *>(Updater::instance());
#endif
    if (ConfigFile().skipUpdateCheck()) {
        updater = nullptr; // don't show update info if updates are disabled
    }

    if (updater) {
        _ui->updateButton->setEnabled(false);

        updater->checkForUpdate();
    }
}

void GeneralSettings::slotToggleAutoUpdateCheck()
{
    ConfigFile cfgFile;
    bool isChecked = _ui->autoCheckForUpdatesCheckBox->isChecked();
    cfgFile.setAutoUpdateCheck(isChecked, QString());
}
#endif // defined(BUILD_UPDATER)

void GeneralSettings::saveMiscSettings()
{
    if (_currentlyLoading) {
        return;
    }

    ConfigFile cfgFile;

    const auto useMonoIcons = _ui->monoIconsCheckBox->isChecked();
    const auto newFolderLimitEnabled = _ui->newFolderLimitCheckBox->isChecked();
    const auto existingFolderLimitEnabled = newFolderLimitEnabled && _ui->existingFolderLimitCheckBox->isChecked();
    const auto stopSyncingExistingFoldersOverLimit = existingFolderLimitEnabled && _ui->stopExistingFolderNowBigSyncCheckBox->isChecked();
    Theme::instance()->setSystrayUseMonoIcons(useMonoIcons);

    cfgFile.setMonoIcons(useMonoIcons);
    cfgFile.setCrashReporter(_ui->crashreporterCheckBox->isChecked());
    cfgFile.setMoveToTrash(_ui->moveFilesToTrashCheckBox->isChecked());
    cfgFile.setNewBigFolderSizeLimit(newFolderLimitEnabled, _ui->newFolderLimitSpinBox->value());
    cfgFile.setConfirmExternalStorage(_ui->newExternalStorage->isChecked());
    cfgFile.setNotifyExistingFoldersOverLimit(existingFolderLimitEnabled);
    cfgFile.setStopSyncingExistingFoldersOverLimit(stopSyncingExistingFoldersOverLimit);

    _ui->existingFolderLimitCheckBox->setEnabled(newFolderLimitEnabled);
    _ui->stopExistingFolderNowBigSyncCheckBox->setEnabled(existingFolderLimitEnabled);
}

void GeneralSettings::slotToggleLaunchOnStartup(bool enable)
{
    const auto theme = Theme::instance();
    if (enable == Utility::hasLaunchOnStartup(theme->appName())) {
        return;
    }

    ConfigFile configFile;
    configFile.setLaunchOnSystemStartup(enable);
    Utility::setLaunchOnStartup(theme->appName(), theme->appNameGUI(), enable);
}

void GeneralSettings::slotToggleOptionalServerNotifications(bool enable)
{
    ConfigFile cfgFile;
    cfgFile.setOptionalServerNotifications(enable);
    _ui->chatNotificationsCheckBox->setEnabled(enable);
    _ui->callNotificationsCheckBox->setEnabled(enable);
}

void GeneralSettings::slotToggleChatNotifications(bool enable)
{
    ConfigFile cfgFile;
    cfgFile.setShowChatNotifications(enable);
}

void GeneralSettings::slotToggleCallNotifications(bool enable)
{
    ConfigFile cfgFile;
    cfgFile.setShowCallNotifications(enable);
}

void GeneralSettings::slotShowInExplorerNavigationPane(bool checked)
{
    ConfigFile cfgFile;
    cfgFile.setShowInExplorerNavigationPane(checked);
    // Now update the registry with the change.
    FolderMan::instance()->navigationPaneHelper().setShowInExplorerNavigationPane(checked);
}

void GeneralSettings::slotIgnoreFilesEditor()
{
    if (_ignoreEditor.isNull()) {
        ConfigFile cfgFile;
        _ignoreEditor = new IgnoreListEditor(this);
        _ignoreEditor->setAttribute(Qt::WA_DeleteOnClose, true);
        _ignoreEditor->open();
    } else {
        ownCloudGui::raiseDialog(_ignoreEditor);
    }
}

void GeneralSettings::slotCreateDebugArchive()
{
    const auto filename = QFileDialog::getSaveFileName(
        this,
        tr("Create Debug Archive"),
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        tr("Zip Archives") + " (*.zip)"
    );

    if (filename.isEmpty()) {
        return;
    }

    if (createDebugArchive(filename)) {
        QMessageBox::information(
            this,
            tr("Debug Archive Created"),
            tr("Debug archive is created at %1").arg(filename)
        );
    }
}

void GeneralSettings::slotShowLegalNotice()
{
    auto notice = new LegalNotice();
    notice->exec();
    delete notice;
}

void GeneralSettings::slotStyleChanged()
{
    customizeStyle();
}

void GeneralSettings::customizeStyle()
{
    // setup about section
    const auto aboutText = []() {
        auto aboutText = Theme::instance()->about();
        Theme::replaceLinkColorStringBackgroundAware(aboutText);
        return aboutText;
    }();
    _ui->infoAndUpdatesLabel->setText(aboutText);

#if defined(BUILD_UPDATER)
    // updater info
    slotUpdateInfo();
#else
    _ui->updatesContainer->setVisible(false);
#endif
}

} // namespace OCC
