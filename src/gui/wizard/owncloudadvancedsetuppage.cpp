/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 * Copyright (C) by Krzesimir Nowak <krzesimir@endocode.com>
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

#include <QDir>
#include <QFileDialog>
#include <QUrl>
#include <QTimer>
#include <QStorageInfo>
#include <QMessageBox>
#include <QJsonObject>

#include "QProgressIndicator.h"

 #include "buttonstyle.h"
#include "account.h"
#include "configfile.h"
#include "creds/abstractcredentials.h"
#include "ionostheme.h"
#include "networkjobs.h"
#include "selectivesyncdialog.h"
#include "theme.h"
#include "wizard/owncloudadvancedsetuppage.h"
#include "wizard/owncloudwizard.h"
#include "wizard/owncloudwizardcommon.h"
#include "SesComponents/syncdirvalidation.h"
#include <folderman.h>

namespace OCC {

OwncloudAdvancedSetupPage::OwncloudAdvancedSetupPage(OwncloudWizard *wizard)
    : QWizardPage()
    , _progressIndi(new QProgressIndicator(this))
    , _ocWizard(wizard)
{
    _ui.setupUi(this);
    setupResoultionWidget();

    _filePathLabel.reset(new ElidedLabel);
    _filePathLabel->setObjectName("filePathLabel");
    _filePathLabel->setWordWrap(true);
    _filePathLabel->setElideMode(Qt::ElideMiddle);
    _filePathLabel->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    _filePathLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    _ui.locationsVBox->insertWidget(3, _filePathLabel.data());

    _filePathLabel->setTextFormat(Qt::PlainText);
    _ui.userNameLabel->setTextFormat(Qt::PlainText);
    _ui.serverAddressLabel->setTextFormat(Qt::PlainText);
    _ui.localFolderDescriptionLabel->setTextFormat(Qt::PlainText);
    _ui.confCheckBoxSize->hide();
    _ui.confSpinBox->hide();
    _ui.confTraillingSizeLabel->hide();
    _ui.wSyncStrategy->removeItem(_ui.horizontalLayout_6);


    registerField(QLatin1String("OCSyncFromScratch"), _ui.cbSyncFromScratch);

    auto sizePolicy = _progressIndi->sizePolicy();
    sizePolicy.setRetainSizeWhenHidden(true);
    _progressIndi->setSizePolicy(sizePolicy);

    _ui.resultLayout->addWidget(_progressIndi);
    stopSpinner();
    setupCustomization();

    connect(_ui.pbSelectLocalFolder, &QAbstractButton::clicked, this, &OwncloudAdvancedSetupPage::slotSelectFolder);
    _ui.pbSelectLocalFolder->setProperty("buttonStyle", QVariant::fromValue(OCC::ButtonStyleName::Secondary));
    setButtonText(QWizard::FinishButton, tr("Connect"));

    if (Theme::instance()->enforceVirtualFilesSyncFolder()) {
        _ui.rSyncEverything->setDisabled(true);
        _ui.rSelectiveSync->setDisabled(true);
    }

    connect(_ui.rSyncEverything, &QAbstractButton::clicked, this, &OwncloudAdvancedSetupPage::slotSyncEverythingClicked);
    connect(_ui.rSelectiveSync, &QAbstractButton::clicked, this, &OwncloudAdvancedSetupPage::slotSelectiveSyncClicked);
    connect(_ui.rVirtualFileSync, &QAbstractButton::clicked, this, &OwncloudAdvancedSetupPage::slotVirtualFileSyncClicked);
    connect(_ui.rVirtualFileSync, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) {
            _ui.lSelectiveSyncSizeLabel->clear();
            _selectiveSyncBlacklist.clear();
        }
    });

    const auto theme = Theme::instance();
    const auto appIcon = theme->applicationIcon();
    const auto appIconSize = Theme::isHidpi() ? 128 : 64;

    _ui.lServerIcon->setPixmap(appIcon.pixmap(appIconSize));

    if (theme->wizardHideExternalStorageConfirmationCheckbox()) {
        //_ui.confCheckBoxExternal->hide();
    }
    if (theme->wizardHideFolderSizeLimitCheckbox()) {
        _ui.confCheckBoxSize->hide();
        _ui.confSpinBox->hide();
        _ui.confTraillingSizeLabel->hide();
    }

    _ui.rVirtualFileSync->setText(tr("Use &virtual files instead of downloading content immediately %1").arg(bestAvailableVfsMode() == Vfs::WindowsCfApi ? QString() : tr("(experimental)")));
#ifdef Q_OS_WIN
    if (bestAvailableVfsMode() == Vfs::WindowsCfApi) {
        _ui.wSyncStrategy->addLayout(_ui.lVirtualFileSync);
        setRadioChecked(_ui.rVirtualFileSync);
    }
#endif
}

void OwncloudAdvancedSetupPage::setupCustomization()
{
    // set defaults for the customize labels.
    _ui.topLabel->hide();
    _ui.bottomLabel->hide();

    Theme *theme = Theme::instance();
    QVariant variant = theme->customMedia(Theme::oCSetupTop);
    if (!variant.isNull()) {
        WizardCommon::setupCustomMedia(variant, _ui.topLabel);
    }

    variant = theme->customMedia(Theme::oCSetupBottom);
    WizardCommon::setupCustomMedia(variant, _ui.bottomLabel);

    WizardCommon::customizeHintLabel(_filePathLabel.data());
    WizardCommon::customizeHintLabel(_ui.lFreeSpace);
    WizardCommon::customizeHintLabel(_ui.lSyncEverythingSizeLabel);
    WizardCommon::customizeHintLabel(_ui.lSelectiveSyncSizeLabel);
    WizardCommon::customizeHintLabel(_ui.serverAddressLabel);
}

bool OwncloudAdvancedSetupPage::isComplete() const
{
    return !_checking && _localFolderValid;
}

void OwncloudAdvancedSetupPage::initializePage()
{
    WizardCommon::initErrorLabel(_ui.errorLabel);

    if (!Theme::instance()->showVirtualFilesOption() || bestAvailableVfsMode() == Vfs::Off) {
        // If the layout were wrapped in a widget, the auto-grouping of the
        // radio buttons no longer works and there are surprising margins.
        // Just manually hide the button and remove the layout.
        _ui.rVirtualFileSync->hide();
        _ui.wSyncStrategy->layout()->removeItem(_ui.lVirtualFileSync);
    }

    _checking = false;
    _ui.lSelectiveSyncSizeLabel->clear();
    _ui.lSyncEverythingSizeLabel->clear();

    // Update the local folder - this is not guaranteed to find a good one
    ConfigFile cfg;
    const auto overrideLocalDir = !cfg.overrideLocalDir().isEmpty();

    auto goodLocalFolder = FolderMan::instance()->findGoodPathForNewSyncFolder(localFolder(), serverUrl(), FolderMan::GoodPathStrategy::AllowOnlyNewPath);
    if (overrideLocalDir) {
        ConfigFile cfg;
        goodLocalFolder = FolderMan::instance()->findGoodPathForNewSyncFolder(cfg.overrideLocalDir(), serverUrl(), FolderMan::GoodPathStrategy::AllowOverrideExistingPath);
    }
    wizard()->setProperty("localFolder", goodLocalFolder);

    // call to init label
    updateStatus();

    // ensure "next" gets the focus, not obSelectLocalFolder
    QTimer::singleShot(0, wizard()->button(QWizard::FinishButton), qOverload<>(&QWidget::setFocus));

    auto acc = dynamic_cast<OwncloudWizard *>(wizard())->account();
    auto quotaJob = new PropfindJob(acc, _remoteFolder, this);
    quotaJob->setProperties(QList<QByteArray>() << "http://owncloud.org/ns:size");

    connect(quotaJob, &PropfindJob::result, this, &OwncloudAdvancedSetupPage::slotQuotaRetrieved);
    quotaJob->start();


    if (Theme::instance()->wizardSelectiveSyncDefaultNothing()) {
        _selectiveSyncBlacklist = QStringList("/");
        setRadioChecked(_ui.rSelectiveSync);
        QTimer::singleShot(0, this, &OwncloudAdvancedSetupPage::slotSelectiveSyncClicked);
    }

    ConfigFile cfgFile;
    auto newFolderLimit = cfgFile.newBigFolderSizeLimit();
    _ui.confCheckBoxSize->setChecked(newFolderLimit.first);
    _ui.confSpinBox->setValue(newFolderLimit.second);
    //_ui.confCheckBoxExternal->setChecked(cfgFile.confirmExternalStorage());

    SetAvatarIcon();
    setUserInformation();

    customizeStyle();

    auto nextButton = qobject_cast<QPushButton *>(_ocWizard->button(QWizard::NextButton));
    if (nextButton) {
        nextButton->setDefault(true);
    }
    if (Theme::instance()->forceOverrideServerUrl()) {
        QTimer::singleShot(0, this, [this]() {
            ConfigFile cfg;
            connect(_ocWizard, &QDialog::accepted, [&]() {
                cfg.setOverrideServerUrl({});
                cfg.setOverrideLocalDir({});
            });
            if (!cfg.overrideLocalDir().isEmpty()) {
                _ocWizard->accept();
            }
        });
    }
}

void OwncloudAdvancedSetupPage::SetAvatarIcon()
{
    const auto icon = QIcon(IonosTheme::avatarIcon());
     _ui.lServerIcon->setPixmap(icon.pixmap(32));
}

void OwncloudAdvancedSetupPage::fetchUserAvatar()
{
    // Reset user avatar
    const auto appIcon = Theme::instance()->applicationIcon();
    // To match the folder icon opposite the avatar -- that is 60x60, minus padding
    _ui.lServerIcon->setPixmap(appIcon.pixmap(32));
    // Fetch user avatar
    const auto account = _ocWizard->account();
    auto avatarSize = 32;
    if (Theme::isHidpi()) {
        avatarSize *= 2;
    }
    const auto avatarJob = new AvatarJob(account, account->davUser(), avatarSize, this);
    avatarJob->setTimeout(20 * 1000);
    QObject::connect(avatarJob, &AvatarJob::avatarPixmap, this, [this](const QImage &avatarImage) {
        if (avatarImage.isNull()) {
            return;
        }
        const auto avatarPixmap = QPixmap::fromImage(AvatarJob::makeCircularAvatar(avatarImage));
        _ui.lServerIcon->setPixmap(avatarPixmap);
    });
    avatarJob->start();
}

void OwncloudAdvancedSetupPage::setUserInformation()
{
    const auto account = _ocWizard->account();
    const auto serverUrl = account->url().toString();
    setServerAddressLabelUrl(serverUrl);
    const auto userName = account->davDisplayName();
    _ui.userNameLabel->setText(userName);
}

void OwncloudAdvancedSetupPage::refreshVirtualFilesAvailibility(const QString &path)
{
    // TODO: remove when UX decision is made
    if (!_ui.rVirtualFileSync->isVisible()) {
        return;
    }

    if (Utility::isPathWindowsDrivePartitionRoot(path)) {
        _ui.rVirtualFileSync->setText(tr("Virtual files are not supported for Windows partition roots as local folder. Please choose a valid subfolder under drive letter."));
        setRadioChecked(_ui.rSyncEverything);
        _ui.rVirtualFileSync->setEnabled(false);
    } else {
        _ui.rVirtualFileSync->setText(tr("Use &virtual files instead of downloading content immediately %1").arg(bestAvailableVfsMode() == Vfs::WindowsCfApi ? QString() : tr("(experimental)")));
        _ui.rVirtualFileSync->setEnabled(true);
    }
    //
}

void OwncloudAdvancedSetupPage::setServerAddressLabelUrl(const QUrl &url)
{
    if (!url.isValid()) {
        return;
    }

    const auto prettyUrl = url.toString().mid(url.scheme().size() + 3); // + 3 because we need to remove ://
    _ui.serverAddressLabel->setText(prettyUrl);
}

// Called if the user changes the user- or url field. Adjust the texts and
// evtl. warnings on the dialog.
void OwncloudAdvancedSetupPage::updateStatus()
{
    const QString locFolder = localFolder();

    // check if the local folder exists. If so, and if its not empty, show a warning.
    const auto pathValidityCheckResult = FolderMan::instance()->checkPathValidityForNewFolder(locFolder, serverUrl());
    auto errorStr = pathValidityCheckResult.second;
    _localFolderValid = errorStr.isEmpty() || pathValidityCheckResult.first == FolderMan::PathValidityResult::ErrorNonEmptyFolder;

    QString t;

    if (dataChanged()) {
        if (_remoteFolder.isEmpty() || _remoteFolder == QLatin1String("/")) {
            t = "";
        } else {
            t = Utility::escape(tr(R"(%1 folder "%2" is synced to local folder "%3")")
                                    .arg(Theme::instance()->appName(), _remoteFolder,
                                        QDir::toNativeSeparators(locFolder)));
            _ui.rSyncEverything->setText(tr("Sync the folder \"%1\"").arg(_remoteFolder));
        }

        const bool dirNotEmpty(QDir(locFolder).entryList(QDir::AllEntries | QDir::NoDotAndDotDot).count() > 0);
        if (dirNotEmpty) {
            t += tr("Warning: The local folder is not empty. Pick a resolution!");
        }
        setResolutionGuiVisible(dirNotEmpty);
    } else {
        setResolutionGuiVisible(false);
    }

    _filePathLabel->setText(QDir::toNativeSeparators(locFolder));

    QString lfreeSpaceStr = Utility::octetsToString(availableLocalSpace());
    _ui.lFreeSpace->setText(QString(tr("%1 free space", "%1 gets replaced with the size and a matching unit. Example: 3 MB or 5 GB")).arg(lfreeSpaceStr));

    _ui.syncModeLabel->setText(t);

    qint64 rSpace = _ui.rSyncEverything->isChecked() ? _rSize : _rSelectedSize;

    QString spaceError = checkLocalSpace(rSpace);
    if (!spaceError.isEmpty()) {
        errorStr = spaceError;
    }
    setErrorString(errorStr);

    emit completeChanged();
}

void OwncloudAdvancedSetupPage::setResolutionGuiVisible(bool value)
{
    _ui.syncModeLabel->setVisible(value);
    _ui.rKeepLocal->setVisible(value);
    _ui.cbSyncFromScratch->setVisible(value);
    _ocWizard->adjustSize();
}

/* obsolete */
bool OwncloudAdvancedSetupPage::dataChanged()
{
    return true;
}

void OwncloudAdvancedSetupPage::startSpinner()
{
    _ui.resultLayout->setEnabled(true);
    _progressIndi->setVisible(true);
    _progressIndi->startAnimation();
}

void OwncloudAdvancedSetupPage::stopSpinner()
{
    _ui.resultLayout->setEnabled(false);
    _progressIndi->setVisible(false);
    _progressIndi->stopAnimation();
}

QUrl OwncloudAdvancedSetupPage::serverUrl() const
{
    const auto urlString = dynamic_cast<OwncloudWizard *>(wizard())->ocUrl();
    const auto user = dynamic_cast<OwncloudWizard *>(wizard())->getCredentials()->user();

    QUrl url(urlString);
    url.setUserName(user);
    return url;
}

int OwncloudAdvancedSetupPage::nextId() const
{
    // tells the caller that this is the last dialog page
    return -1;
}

QString OwncloudAdvancedSetupPage::localFolder() const
{
    QString folder = wizard()->property("localFolder").toString();
    return folder;
}

QStringList OwncloudAdvancedSetupPage::selectiveSyncBlacklist() const
{
    return _selectiveSyncBlacklist;
}

bool OwncloudAdvancedSetupPage::useVirtualFileSync() const
{
    return _ui.rVirtualFileSync->isChecked();
}

bool OwncloudAdvancedSetupPage::isConfirmBigFolderChecked() const
{
    return _ui.rSyncEverything->isChecked() && _ui.confCheckBoxSize->isChecked();
}

bool OwncloudAdvancedSetupPage::validatePage()
{
    if (useVirtualFileSync()) {
        const auto availability = Vfs::checkAvailability(localFolder(), bestAvailableVfsMode());
        if (!availability) {
            auto msg = new QMessageBox(QMessageBox::Warning,
                                       tr("Virtual files are not supported at the selected location"),
                                       availability.error(),
                                       QMessageBox::Ok, this);
            msg->setAttribute(Qt::WA_DeleteOnClose);
            msg->open();
            return false;
        }
    }

    if (!_created) {
        setErrorString(QString());
        _checking = true;
        startSpinner();
        emit completeChanged();
        ConfigFile cfgFile;
        if (_ui.rSyncEverything->isChecked()) {
            ConfigFile cfgFile;
            cfgFile.setNewBigFolderSizeLimit(_ui.confCheckBoxSize->isChecked(),
                _ui.confCheckBoxSize->isChecked() ? _ui.confSpinBox->value() : -1);
            //cfgFile.setConfirmExternalStorage(_ui.confCheckBoxExternal->isChecked());
        }
        else
        {
            cfgFile.setNewBigFolderSizeLimit(false, -1);
            cfgFile.setConfirmExternalStorage(false);
        }

        emit createLocalAndRemoteFolders(localFolder(), _remoteFolder);
        return false;
    } else {
        // connecting is running
        _checking = false;
        emit completeChanged();
        stopSpinner();
        return true;
    }
}

void OwncloudAdvancedSetupPage::setErrorString(const QString &err)
{
    if (err.isEmpty()) {
        _ui.errorLabel->setVisible(false);
    } else {
        _ui.errorLabel->setVisible(true);
        _ui.errorLabel->setText(err);
    }
    _ocWizard->adjustSize();
    _checking = false;
    emit completeChanged();
}

void OwncloudAdvancedSetupPage::directoriesCreated()
{
    _checking = false;
    _created = true;
    stopSpinner();
    emit completeChanged();
}

void OwncloudAdvancedSetupPage::setRemoteFolder(const QString &remoteFolder)
{
    if (!remoteFolder.isEmpty()) {
        _remoteFolder = remoteFolder;
    }
}

void OwncloudAdvancedSetupPage::slotSelectFolder()
{
    QString dir = QFileDialog::getExistingDirectory(nullptr, tr("Local Sync Folder"), QDir::homePath());
        SyncDirValidator syncDirValidator(dir);
    if (!syncDirValidator.isValidDir()) {
        setErrorString(syncDirValidator.message());
        return;
    }
    if (!dir.isEmpty()) {
        // TODO: remove when UX decision is made
        refreshVirtualFilesAvailibility(dir);

        wizard()->setProperty("localFolder", dir);
        updateStatus();
    }

    qint64 rSpace = _ui.rSyncEverything->isChecked() ? _rSize : _rSelectedSize;
    QString errorStr = checkLocalSpace(rSpace);
    setErrorString(errorStr);
}

void OwncloudAdvancedSetupPage::slotSelectiveSyncClicked()
{
    AccountPtr acc = dynamic_cast<OwncloudWizard *>(wizard())->account();
    auto *dlg = new SelectiveSyncDialog(acc, _remoteFolder, _selectiveSyncBlacklist, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);

    connect(dlg, &SelectiveSyncDialog::finished, this, [this, dlg]{
        const int result = dlg->result();
        bool updateBlacklist = false;

        // We need to update the selective sync blacklist either when the dialog
        // was accepted in that
        // case the stub blacklist of / was expanded to the actual list of top
        // level folders by the selective sync dialog.
        if (result == QDialog::Accepted) {
            _selectiveSyncBlacklist = dlg->createBlackList();
            updateBlacklist = true;
            // _ui.confCheckBoxExternal->setStyleSheet(IonosTheme::fontConfigurationCss(
            //     IonosTheme::settingsFont(),
            //     IonosTheme::settingsTextSize(),
            //     IonosTheme::settingsTextWeight(),
            //     IonosTheme::loginWizardFontLightGrey()
            // ));

            _ui.confCheckBoxSize->setDisabled(true);
        } else if (result == QDialog::Rejected && _selectiveSyncBlacklist == QStringList("/")) {
            _selectiveSyncBlacklist = dlg->oldBlackList();
            updateBlacklist = true;
        }

        if (updateBlacklist) {
            if (!_selectiveSyncBlacklist.isEmpty()) {
                _ui.rSelectiveSync->blockSignals(true);
                setRadioChecked(_ui.rSelectiveSync);
                _ui.rSelectiveSync->blockSignals(false);
                auto s = dlg->estimatedSize();
                if (s > 0) {
                    _ui.lSelectiveSyncSizeLabel->setText(tr("(%1)").arg(Utility::octetsToString(s)));
                } else {
                    _ui.lSelectiveSyncSizeLabel->setText(QString());
                }
            } else {
                setRadioChecked(_ui.rSyncEverything);
                _ui.lSelectiveSyncSizeLabel->setText(QString());
            }
            wizard()->setProperty("blacklist", _selectiveSyncBlacklist);
        }

        updateStatus();

    });
    dlg->open();


}

void OwncloudAdvancedSetupPage::slotVirtualFileSyncClicked()
{
    if (!_ui.rVirtualFileSync->isChecked()) {
        OwncloudWizard::askExperimentalVirtualFilesFeature(this, [this](bool enable) {
            if (!enable)
                return;
            setRadioChecked(_ui.rVirtualFileSync);
        });
    }

    // _ui.confCheckBoxExternal->setStyleSheet(IonosTheme::fontConfigurationCss(
    //             IonosTheme::settingsFont(),
    //             IonosTheme::settingsTextSize(),
    //             IonosTheme::settingsTextWeight(),
    //             IonosTheme::loginWizardFontLightGrey()
    //         ));

    _ui.confCheckBoxSize->setDisabled(true);
}

void OwncloudAdvancedSetupPage::slotSyncEverythingClicked()
{
    _ui.lSelectiveSyncSizeLabel->setText(QString());
    setRadioChecked(_ui.rSyncEverything);
    _selectiveSyncBlacklist.clear();

    QString errorStr = checkLocalSpace(_rSize);
    setErrorString(errorStr);

    // _ui.confCheckBoxExternal->setStyleSheet(IonosTheme::fontConfigurationCss(
    //     IonosTheme::settingsFont(),
    //     IonosTheme::settingsTextSize(),
    //     IonosTheme::settingsTextWeight(),
    //     IonosTheme::titleColor()
    // ));
}


void OwncloudAdvancedSetupPage::slotQuotaRetrieved(const QVariantMap &result)
{
    _rSize = result["size"].toDouble();
    _ui.lSyncEverythingSizeLabel->setText(tr("(%1)").arg(Utility::octetsToString(_rSize)));

    updateStatus();
}

qint64 OwncloudAdvancedSetupPage::availableLocalSpace() const
{
    QString localDir = localFolder();
    QString path = !QDir(localDir).exists() && localDir.contains(QDir::homePath()) ?
                QDir::homePath() : localDir;
    QStorageInfo storage(QDir::toNativeSeparators(path));

    return storage.bytesAvailable();
}

QString OwncloudAdvancedSetupPage::checkLocalSpace(qint64 remoteSize) const
{
    return (availableLocalSpace()>remoteSize) ? QString() : tr("There isn't enough free space in the local folder!");
}

void OwncloudAdvancedSetupPage::slotStyleChanged()
{
    customizeStyle();
}

void OwncloudAdvancedSetupPage::customizeStyle()
{
    if (_progressIndi) {
        const auto isDarkBackground = Theme::isDarkColor(palette().window().color());
        if (isDarkBackground) {
            _progressIndi->setColor(Qt::white);
        } else {
            _progressIndi->setColor(Qt::black);
        }
    }

    _ocWizard->setMinimumSize(QSize(626, 515));
    _ocWizard->setMaximumSize(QSize(626, 600));

    _ui.mainHBox->setContentsMargins(0, 0, 0, 0);
    _ui.wSyncStrategySynchronizeEverything->setContentsMargins(0, 0, 0, 0);
    _ui.lVirtualFileSync->setContentsMargins(0, 0, 0, 0);
    //_ui.horizontalLayout_8->setContentsMargins(32, 0, 0, 0);
    _ui.horizontalLayout_10->setContentsMargins(0, 8, 0, 0);

    _ui.wSyncStrategy->setSpacing(16);
    _ui.wSyncStrategy->setContentsMargins(0, 0, 0, 0);
    _ui.verticalLayout->setSpacing(0);
    _ui.pbSelectLocalFolder->setMinimumSize(0, 40);

    _ui.serverVBox->setAlignment(Qt::AlignTop);
    _ui.serverVBox->setSpacing(5);
    _ui.serverVBox->setContentsMargins(0, 0, 0, 0);

    _ui.arrowVBox->setSpacing(0);
    _ui.arrowVBox->setContentsMargins(0, 0, 0, 0);
    _ui.arrowVBox->setAlignment(Qt::AlignTop);

    _ui.locationsVBox->setAlignment(Qt::AlignTop);
    _ui.locationsVBox->setContentsMargins(0, 0, 0, 0);
    _ui.locationsVBox->setSpacing(5);

    _ui.resolutionWidgetLayout->setContentsMargins(0, 0, 0, 0);

    styleSyncLogo();
    styleLocalFolderLabel();

    setStyleSheet(
        "QRadioButton {" +
            IonosTheme::fontConfigurationCss(
                IonosTheme::settingsFont(),
                IonosTheme::settingsTextSize(),
                IonosTheme::settingsTextWeight(),
                IonosTheme::titleColor()
            ) + "} QCheckBox {" +
            IonosTheme::fontConfigurationCss(
                IonosTheme::settingsFont(),
                IonosTheme::settingsTextSize(),
                IonosTheme::settingsTextWeight(),
                IonosTheme::titleColor()
            ) + "}"
    );

    _ui.userNameLabel->setStyleSheet(IonosTheme::fontConfigurationCss(
        IonosTheme::settingsFont(),
        IonosTheme::settingsTextSize(),
        IonosTheme::settingsTextWeight(),
        IonosTheme::titleColor()
    ));

    _ui.serverAddressLabel->setStyleSheet(IonosTheme::fontConfigurationCss(
        IonosTheme::settingsFont(),
        IonosTheme::settingsTextSize(),
        IonosTheme::settingsTextWeight(),
        IonosTheme::loginWizardFontGrey()
    ));

    _ui.localFolderDescriptionLabel->setStyleSheet(IonosTheme::fontConfigurationCss(
        IonosTheme::settingsFont(),
        IonosTheme::settingsTextSize(),
        IonosTheme::settingsTextWeight(),
        IonosTheme::titleColor()
    ));

    _filePathLabel->setStyleSheet(IonosTheme::fontConfigurationCss(
        IonosTheme::settingsFont(),
        IonosTheme::settingsTextSize(),
        IonosTheme::settingsTextWeight(),
        IonosTheme::loginWizardFontGrey()
    ));

    _ui.lFreeSpace->setStyleSheet(IonosTheme::fontConfigurationCss(
        IonosTheme::settingsFont(),
        IonosTheme::settingsTextSize(),
        IonosTheme::settingsTextWeight(),
        IonosTheme::loginWizardFontGrey()
    ));

    #ifdef Q_OS_MAC
    _ui.syncModeLabel->setStyleSheet(
                        IonosTheme::fontConfigurationCss(
                        IonosTheme::settingsFont(),
                        IonosTheme::settingsTextSize(),
                        IonosTheme::settingsTitleWeight600(),
                        IonosTheme::titleColor()
                        )
                );
    #endif

}

void OwncloudAdvancedSetupPage::styleLocalFolderLabel()
{
    const auto icon = QIcon(IonosTheme::folderIcon());
     _ui.lLocal->setPixmap(icon.pixmap(32));
}

void OwncloudAdvancedSetupPage::setRadioChecked(QRadioButton *radio)
{
    // We don't want clicking the radio buttons to immediately adjust the checked state
    // for selective sync and virtual file sync, so we keep them uncheckable until
    // they should be checked.
    radio->setCheckable(true);
    radio->setChecked(true);

    if (radio != _ui.rSelectiveSync)
        _ui.rSelectiveSync->setCheckable(false);
    if (radio != _ui.rVirtualFileSync)
        _ui.rVirtualFileSync->setCheckable(false);
}

void OwncloudAdvancedSetupPage::styleSyncLogo()
{
    const auto syncArrowIcon = QIcon(IonosTheme::syncArrows());
    // const auto syncArrowIcon = Theme::createColorAwareIcon(QLatin1String(":/client/theme/sync-arrow.svg"), palette());
    _ui.syncLogoLabel->setPixmap(syncArrowIcon.pixmap(QSize(32,32)));
    _ui.syncLogoLabel->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
}

void OwncloudAdvancedSetupPage::setupResoultionWidget()
{
    for (int i = 0; i < _ui.resolutionWidgetLayout->count(); ++i) {
        auto widget = _ui.resolutionWidgetLayout->itemAt(i)->widget();
        if (!widget) {
            continue;
        }

        auto sizePolicy = widget->sizePolicy();
        sizePolicy.setRetainSizeWhenHidden(true);
        widget->setSizePolicy(sizePolicy);
    }
}

} // namespace OCC
