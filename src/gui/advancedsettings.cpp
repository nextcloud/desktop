/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "advancedsettings.h"
#include "ui_advancedsettings.h"

#include "accountmanager.h"
#include "application.h"
#include "capabilities.h"
#include "common/utility.h"
#include "configfile.h"
#include "folderman.h"
#include "folder.h"
#include "ignorelisteditor.h"
#include "logger.h"
#include "owncloudgui.h"
#include "settingspanelstyle.h"
#include "theme.h"
#include "common/syncjournaldb.h"

#ifdef BUILD_FILE_PROVIDER_MODULE
#include "macOS/fileproviderutils.h"
#endif

#ifdef Q_OS_MACOS
#include "common/utility_mac_sandbox.h"
#endif

#include <KZip>

#include <QDir>
#include <QDirIterator>
#include <QAbstractButton>
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QLabel>
#include <QMessageBox>
#include <QOperatingSystemVersion>
#include <QScopedValueRollback>
#include <QSpinBox>
#include <QStandardPaths>
#include <QVector>

#include <algorithm>
#include <chrono>

Q_LOGGING_CATEGORY(lcAdvancedSettings, "nextcloud.settings.advanced")

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
    const auto entries = createDebugArchiveFileList();

    const auto tempDir = QDir::temp();
    const auto tempFilePath = tempDir.filePath(QStringLiteral("nextcloud-debug-archive-temp.zip"));

    KZip zip(tempFilePath);

    if (!zip.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to open debug archive for writing:"
                 << tempFilePath
                 << "because of error:"
                 << zip.errorString();

        QMessageBox::critical(
            nullptr,
            QObject::tr("Failed to create debug archive"),
            QObject::tr("Could not create debug archive in selected location!"),
            QMessageBox::Ok
        );

        return false;
    }

    for (const auto &entry : entries) {
        zip.addLocalFile(entry.localFilename, entry.zipFilename);
    }

#ifdef BUILD_FILE_PROVIDER_MODULE
    qDebug() << "Trying to add file provider domain database and log files...";
    const auto fileProviderDomainsSupportDirectory = OCC::Mac::FileProviderUtils::fileProviderDomainsSupportDirectory();

    if (fileProviderDomainsSupportDirectory.exists()) {
        QDirIterator it(fileProviderDomainsSupportDirectory.path(), QStringList() << "*.jsonl" << "*.realm", QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);

        while (it.hasNext()) {
            const auto filePath = it.next();
            const auto relativePath = fileProviderDomainsSupportDirectory.relativeFilePath(filePath);
            const auto zipPath = QStringLiteral("File Provider Domains/%1").arg(relativePath);

            zip.addLocalFile(filePath, zipPath);
            qDebug() << "Added file from" << filePath;
        }
    } else {
        qWarning() << "file provider domain container log directory not found at" << fileProviderDomainsSupportDirectory.path();
    }
#endif

    const auto clientParameters = QCoreApplication::arguments().join(' ').toUtf8();
    zip.prepareWriting("_client_parameters.txt", {}, {}, clientParameters.size());
    zip.writeData(clientParameters, clientParameters.size());
    zip.finishWriting(clientParameters.size());

    const auto buildInfo = QString(OCC::Theme::instance()->aboutInfo() + "\n\n" + OCC::Theme::instance()->aboutDetails()).toUtf8();
    zip.prepareWriting("_client_buildinfo.txt", {}, {}, buildInfo.size());
    zip.writeData(buildInfo, buildInfo.size());
    zip.finishWriting(buildInfo.size());

    zip.close();

    QFile tempFile(tempFilePath);
    if (!tempFile.exists()) {
        qWarning() << "Temporary debug archive file does not exist:" << tempFilePath;
        QMessageBox::critical(
            nullptr,
            QObject::tr("Failed to create debug archive"),
            QObject::tr("Could not create debug archive in temporary location!"),
            QMessageBox::Ok
        );
        return false;
    }

    if (QFile::exists(filename) && !QFile::remove(filename)) {
        qWarning() << "Failed to remove existing file at destination:" << filename;
        tempFile.remove();
        QMessageBox::critical(
            nullptr,
            QObject::tr("Failed to create debug archive"),
            QObject::tr("Could not remove existing file at destination!"),
            QMessageBox::Ok
        );
        return false;
    }

    if (!tempFile.rename(filename)) {
        qWarning() << "Failed to move debug archive from" << tempFilePath << "to" << filename;
        tempFile.remove();
        QMessageBox::critical(
            nullptr,
            QObject::tr("Failed to create debug archive"),
            QObject::tr("Could not move debug archive to selected location!"),
            QMessageBox::Ok
        );
        return false;
    }

    return true;
}

} // namespace

namespace OCC {

AdvancedSettings::AdvancedSettings(QWidget *parent)
    : QWidget(parent)
    , _ui(new Ui::AdvancedSettings)
{
    _ui->setupUi(this);

    auto *advancedActionsLabel = new QLabel(tr("Advanced"), this);
    advancedActionsLabel->setObjectName(QLatin1String("advancedActionsLabel"));
    _ui->advancedActionsLayout->insertWidget(0, advancedActionsLabel);

    connect(_ui->newFolderLimitCheckBox, &QAbstractButton::toggled, this, &AdvancedSettings::saveMiscSettings);
    connect(_ui->newFolderLimitSpinBox, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &AdvancedSettings::saveMiscSettings);
    connect(_ui->existingFolderLimitCheckBox, &QAbstractButton::toggled, this, &AdvancedSettings::saveMiscSettings);
    connect(_ui->stopExistingFolderNowBigSyncCheckBox, &QAbstractButton::toggled, this, &AdvancedSettings::saveMiscSettings);
    connect(_ui->newExternalStorage, &QAbstractButton::toggled, this, &AdvancedSettings::saveMiscSettings);
    connect(_ui->moveFilesToTrashCheckBox, &QAbstractButton::toggled, this, &AdvancedSettings::saveMiscSettings);
    connect(_ui->showInExplorerNavigationPaneCheckBox, &QAbstractButton::toggled, this, &AdvancedSettings::slotShowInExplorerNavigationPane);
    connect(_ui->remotePollIntervalSpinBox, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &AdvancedSettings::slotRemotePollIntervalChanged);
    connect(_ui->ignoredFilesButton, &QAbstractButton::clicked, this, &AdvancedSettings::slotIgnoreFilesEditor);
    connect(_ui->debugArchiveButton, &QAbstractButton::clicked, this, &AdvancedSettings::slotCreateDebugArchive);

#ifdef Q_OS_MACOS
    QString txt = _ui->showInExplorerNavigationPaneLabel->text();
    txt.replace(QString::fromLatin1("Explorer"), QString::fromLatin1("Finder"));
    _ui->showInExplorerNavigationPaneLabel->setText(txt);
#endif

#ifdef Q_OS_WIN
    if (QOperatingSystemVersion::current() < QOperatingSystemVersion::Windows10) {
        _ui->showInExplorerNavigationPaneCheckBox->setVisible(false);
        _ui->showInExplorerNavigationPaneLabel->setVisible(false);
        _ui->showInExplorerNavigationPaneRowWidget->setVisible(false);
        _ui->showInExplorerNavigationPaneSeparator->setVisible(false);
    }
#else
    _ui->showInExplorerNavigationPaneCheckBox->setVisible(false);
    _ui->showInExplorerNavigationPaneLabel->setVisible(false);
    _ui->showInExplorerNavigationPaneRowWidget->setVisible(false);
    _ui->showInExplorerNavigationPaneSeparator->setVisible(false);
#endif

    loadMiscSettings();

    // accountAdded means the wizard was finished and the wizard might change some options.
    connect(AccountManager::instance(), &AccountManager::accountAdded, this, &AdvancedSettings::loadMiscSettings);
    connect(AccountManager::instance(), &AccountManager::accountAdded, this, &AdvancedSettings::updatePollIntervalVisibility);
    connect(AccountManager::instance(), &AccountManager::accountRemoved, this, &AdvancedSettings::updatePollIntervalVisibility);
    connect(AccountManager::instance(), &AccountManager::capabilitiesChanged, this, &AdvancedSettings::updatePollIntervalVisibility);

    customizeStyle();
}

AdvancedSettings::~AdvancedSettings()
{
    delete _ui;
}

QSize AdvancedSettings::sizeHint() const
{
    return {
        ownCloudGui::settingsDialogSize().width(),
        QWidget::sizeHint().height()
    };
}

void AdvancedSettings::loadMiscSettings()
{
    QScopedValueRollback<bool> scope(_currentlyLoading, true);
    ConfigFile cfgFile;

    _ui->newExternalStorage->setChecked(cfgFile.confirmExternalStorage());
    _ui->moveFilesToTrashCheckBox->setChecked(cfgFile.moveToTrash());
    _ui->showInExplorerNavigationPaneCheckBox->setChecked(cfgFile.showInExplorerNavigationPane());

    auto newFolderLimit = cfgFile.newBigFolderSizeLimit();
    _ui->newFolderLimitCheckBox->setChecked(newFolderLimit.first);
    _ui->newFolderLimitSpinBox->setValue(newFolderLimit.second);
    _ui->existingFolderLimitCheckBox->setEnabled(_ui->newFolderLimitCheckBox->isChecked());
    _ui->existingFolderLimitLabel->setEnabled(_ui->newFolderLimitCheckBox->isChecked());
    _ui->existingFolderLimitCheckBox->setChecked(_ui->newFolderLimitCheckBox->isChecked() && cfgFile.notifyExistingFoldersOverLimit());
    _ui->stopExistingFolderNowBigSyncCheckBox->setEnabled(_ui->existingFolderLimitCheckBox->isChecked());
    _ui->stopExistingFolderNowBigSyncLabel->setEnabled(_ui->existingFolderLimitCheckBox->isChecked());
    _ui->stopExistingFolderNowBigSyncCheckBox->setChecked(_ui->existingFolderLimitCheckBox->isChecked() && cfgFile.stopSyncingExistingFoldersOverLimit());

    const auto interval = cfgFile.remotePollInterval();
    _ui->remotePollIntervalSpinBox->setValue(static_cast<int>(interval.count() / 1000));
    updatePollIntervalVisibility();
}

void AdvancedSettings::saveMiscSettings()
{
    if (_currentlyLoading) {
        return;
    }

    ConfigFile cfgFile;

    const auto newFolderLimitEnabled = _ui->newFolderLimitCheckBox->isChecked();
    const auto existingFolderLimitEnabled = newFolderLimitEnabled && _ui->existingFolderLimitCheckBox->isChecked();
    const auto stopSyncingExistingFoldersOverLimit = existingFolderLimitEnabled && _ui->stopExistingFolderNowBigSyncCheckBox->isChecked();

    cfgFile.setMoveToTrash(_ui->moveFilesToTrashCheckBox->isChecked());
    cfgFile.setNewBigFolderSizeLimit(newFolderLimitEnabled, _ui->newFolderLimitSpinBox->value());
    cfgFile.setConfirmExternalStorage(_ui->newExternalStorage->isChecked());
    cfgFile.setNotifyExistingFoldersOverLimit(existingFolderLimitEnabled);
    cfgFile.setStopSyncingExistingFoldersOverLimit(stopSyncingExistingFoldersOverLimit);

    _ui->existingFolderLimitCheckBox->setEnabled(newFolderLimitEnabled);
    _ui->existingFolderLimitLabel->setEnabled(newFolderLimitEnabled);
    _ui->stopExistingFolderNowBigSyncCheckBox->setEnabled(existingFolderLimitEnabled);
    _ui->stopExistingFolderNowBigSyncLabel->setEnabled(existingFolderLimitEnabled);
}

void AdvancedSettings::slotShowInExplorerNavigationPane(bool checked)
{
    ConfigFile cfgFile;
    cfgFile.setShowInExplorerNavigationPane(checked);

#ifdef Q_OS_WIN
    FolderMan::instance()->navigationPaneHelper().setShowInExplorerNavigationPane(checked);
#endif
}

void AdvancedSettings::slotIgnoreFilesEditor()
{
    if (_ignoreEditor.isNull()) {
        _ignoreEditor = new IgnoreListEditor(this);
        _ignoreEditor->setAttribute(Qt::WA_DeleteOnClose, true);
        _ignoreEditor->open();
    } else {
        ownCloudGui::raiseDialog(_ignoreEditor);
    }
}

void AdvancedSettings::slotCreateDebugArchive()
{
    const auto destination = QFileDialog::getSaveFileUrl(
        this,
        tr("Create Debug Archive"),
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        tr("Zip Archives") + " (*.zip)"
    );

    if (!destination.isLocalFile() || destination.toLocalFile().isEmpty()) {
        return;
    }

#ifdef Q_OS_MACOS
    auto scopedAccess = Utility::MacSandboxSecurityScopedAccess::create(destination);

    if (!scopedAccess->isValid()) {
        QMessageBox::critical(
            this,
            tr("Failed to Access File"),
            tr("Could not access the selected location. Please try again or choose a different location.")
        );
        return;
    }
#endif

    if (createDebugArchive(destination.toLocalFile())) {
        QMessageBox::information(
            this,
            tr("Debug Archive Created"),
            tr("Redact information deemed sensitive before sharing! Debug archive created at %1").arg(destination.toLocalFile())
        );
    }
}

void AdvancedSettings::slotRemotePollIntervalChanged(int seconds)
{
    if (_currentlyLoading) {
        return;
    }

    ConfigFile cfgFile;
    std::chrono::milliseconds interval(seconds * 1000);
    cfgFile.setRemotePollInterval(interval);
}

void AdvancedSettings::updatePollIntervalVisibility()
{
    const auto accounts = AccountManager::instance()->accounts();
    const auto pushAvailable = std::any_of(accounts.cbegin(), accounts.cend(), [](const AccountStatePtr &accountState) -> bool {
        if (!accountState) {
            return false;
        }
        const auto accountPtr = accountState->account();
        if (!accountPtr) {
            return false;
        }
        return accountPtr->capabilities().availablePushNotifications().testFlag(PushNotificationType::Files);
    });

    _ui->horizontalLayoutWidget_remotePollInterval->setVisible(!pushAvailable);
    _ui->remotePollIntervalSeparator->setVisible(!pushAvailable);
}

void AdvancedSettings::slotStyleChanged()
{
    customizeStyle();
}

void AdvancedSettings::customizeStyle()
{
    SettingsPanelStyle::apply(this);
}

} // namespace OCC
