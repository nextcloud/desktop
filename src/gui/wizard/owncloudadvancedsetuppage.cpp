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
#include <QMessageBox>

#include "QProgressIndicator.h"

#include "wizard/owncloudwizard.h"
#include "wizard/owncloudwizardcommon.h"
#include "wizard/owncloudadvancedsetuppage.h"
#include "account.h"
#include "theme.h"
#include "configfile.h"
#include "selectivesyncdialog.h"
#include <folderman.h>
#include "creds/abstractcredentials.h"
#include "creds/oauth.h"
#include "networkjobs.h"
#include "guiutility.h"

namespace OCC {

OwncloudAdvancedSetupPage::OwncloudAdvancedSetupPage()
    : QWizardPage()
    , _ui()
    , _checking(false)
    , _created(false)
    , _localFolderValid(false)
    , _progressIndi(new QProgressIndicator(this))
    , _remoteFolder()
{
    _ui.setupUi(this);

    Theme *theme = Theme::instance();
    setTitle(WizardCommon::titleTemplate().arg(tr("Connect to %1").arg(theme->appNameGUI())));
    setSubTitle(WizardCommon::subTitleTemplate().arg(tr("Setup local folder options")));

    registerField(QLatin1String("OCSyncFromScratch"), _ui.cbSyncFromScratch);

    stopSpinner();

    connect(_ui.pbSelectLocalFolder, &QAbstractButton::clicked, this, &OwncloudAdvancedSetupPage::slotSelectFolder);
    setButtonText(QWizard::NextButton, tr("Connect..."));

    connect(_ui.rSyncEverything, &QAbstractButton::clicked, this, &OwncloudAdvancedSetupPage::slotSyncEverythingClicked);
    connect(_ui.rVirtualFileSync, &QAbstractButton::clicked, this, &OwncloudAdvancedSetupPage::slotVirtualFileSyncClicked);
    connect(_ui.rVirtualFileSync, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) {
            _ui.lSelectiveSyncSizeLabel->clear();
            _selectiveSyncBlacklist.clear();
        }
    });
    connect(_ui.bSelectiveSync, &QAbstractButton::clicked, this, &OwncloudAdvancedSetupPage::slotSelectiveSyncClicked);
    connect(_ui.rManualFolder, &QAbstractButton::clicked, this, [this] { setRadioChecked(_ui.rManualFolder); });

    connect(_ui.rSyncEverything, &QRadioButton::toggled, _ui.syncEverythingWidget, &QWidget::setEnabled);
    connect(_ui.rManualFolder, &QRadioButton::toggled, _ui.whereToSyncWidget, &QWidget::setDisabled);

    QIcon appIcon = theme->applicationIcon();
    _ui.lServerIcon->setPixmap(appIcon.pixmap(_ui.lServerIcon->size()));
    if (theme->wizardHideExternalStorageConfirmationCheckbox()) {
        _ui.confCheckBoxExternal->hide();
    }
    if (theme->wizardHideFolderSizeLimitCheckbox()) {
        _ui.confCheckBoxSize->hide();
        _ui.confSpinBox->hide();
        _ui.confTraillingSizeLabel->hide();
    }
    _ui.lLocalIcon->setPixmap(Utility::getCoreIcon(QStringLiteral("folder-sync")).pixmap(_ui.lLocalIcon->size()));

    _ui.rVirtualFileSync->setText(tr("Use &virtual files instead of downloading content immediately%1").arg(bestAvailableVfsMode() == Vfs::WindowsCfApi ? QString() : tr(" (experimental)")));

    connect(this, &OwncloudAdvancedSetupPage::completeChanged, this, [this]{
        if (wizard() && qobject_cast<OwncloudWizard*>(wizard())->authType() == OCC::DetermineAuthTypeJob::AuthType::OAuth) {
            // For OAuth, disable the back button in the Page_AdvancedSetup because we don't want
            // to re-open the browser.
            // HACK: the wizard will reenable the buttons on completeChanged, so delay it
            QTimer::singleShot(0, this, [this]{
                wizard()->button(QWizard::BackButton)->setEnabled(false);
            });
        }
    });
}

bool OwncloudAdvancedSetupPage::isComplete() const
{
    return manualFolderConfig() || (!_checking && _localFolderValid);
}

void OwncloudAdvancedSetupPage::initializePage()
{
    const auto vfsMode = bestAvailableVfsMode();
    if (Theme::instance()->forceVirtualFilesOption() && vfsMode == Vfs::WindowsCfApi) {
        setRadioChecked(_ui.rVirtualFileSync);
        _ui.syncTypeWidget->hide();
    } else {
        if (!Theme::instance()->showVirtualFilesOption() || vfsMode == Vfs::Off || (vfsMode != Vfs::WindowsCfApi && !Theme::instance()->enableExperimentalFeatures())) {
            // If the layout were wrapped in a widget, the auto-grouping of the
            // radio buttons no longer works and there are surprising margins.
            // Just manually hide the button and remove the layout.
            _ui.rVirtualFileSync->hide();
            _ui.wSyncStrategy->layout()->removeItem(_ui.lVirtualFileSync);
        } else {
#ifdef Q_OS_WIN
            if (vfsMode == Vfs::WindowsCfApi) {
                qobject_cast<QVBoxLayout *>(_ui.wSyncStrategy->layout())->insertItem(0, _ui.lVirtualFileSync);
                setRadioChecked(_ui.rVirtualFileSync);
            }
#endif
        }
    }
    _checking = false;
    _ui.lSelectiveSyncSizeLabel->setText(QString());
    _ui.lSyncEverythingSizeLabel->setText(QString());

    // Update the local folder - this is not guaranteed to find a good one
    QString goodLocalFolder = FolderMan::instance()->findGoodPathForNewSyncFolder(localFolder());
    wizard()->setProperty("localFolder", goodLocalFolder);

    // call to init label
    updateStatus();

    // ensure "next" gets the focus, not obSelectLocalFolder
    QTimer::singleShot(0, wizard()->button(QWizard::NextButton), SLOT(setFocus()));

    auto acc = static_cast<OwncloudWizard *>(wizard())->account();
    auto quotaJob = new PropfindJob(acc, _remoteFolder, this);
    quotaJob->setProperties(QList<QByteArray>() << "http://owncloud.org/ns:size");

    connect(quotaJob, &PropfindJob::result, this, &OwncloudAdvancedSetupPage::slotQuotaRetrieved);
    quotaJob->start();

    ConfigFile cfgFile;
    auto newFolderLimit = cfgFile.newBigFolderSizeLimit();
    _ui.confCheckBoxSize->setChecked(newFolderLimit.first);
    _ui.confSpinBox->setValue(newFolderLimit.second);
    _ui.confCheckBoxExternal->setChecked(cfgFile.confirmExternalStorage());
}

// Called if the user changes the user- or url field. Adjust the texts and
// evtl. warnings on the dialog.
void OwncloudAdvancedSetupPage::updateStatus()
{
    const QString locFolder = localFolder();

    // check if the local folder exists. If so, and if its not empty, show a warning.
    QString errorStr = FolderMan::instance()->checkPathValidityForNewFolder(locFolder);
    _localFolderValid = errorStr.isEmpty();

    _ui.pbSelectLocalFolder->setText(QDir::toNativeSeparators(locFolder));
    if (!_remoteFolder.isEmpty() && _remoteFolder != QLatin1String("/")) {
        _ui.rSyncEverything->setText(tr("Sync the folder '%1'").arg(_remoteFolder));
    }

    if (!QDir(locFolder).entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty()) {
        _ui.syncModeLabel->setText(tr("<p><strong>Warning:</strong> The local folder is not empty. "
                                      "Pick a resolution!</p>"));
        _ui.resolutionStackedWidget->setCurrentIndex(1);
    } else {
        _ui.resolutionStackedWidget->setCurrentIndex(0);
    }

    setErrorString(errorStr);
    emit completeChanged();
}

void OwncloudAdvancedSetupPage::startSpinner()
{
    _progressIndi->setVisible(true);
    _progressIndi->startAnimation();
}

void OwncloudAdvancedSetupPage::stopSpinner()
{
    _progressIndi->setVisible(false);
    _progressIndi->stopAnimation();
}

int OwncloudAdvancedSetupPage::nextId() const
{
    return WizardCommon::Page_Result;
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

bool OwncloudAdvancedSetupPage::manualFolderConfig() const
{
    return _ui.rManualFolder->isChecked();
}

bool OwncloudAdvancedSetupPage::isConfirmBigFolderChecked() const
{
    return _ui.rSyncEverything->isChecked() && _ui.confCheckBoxSize->isChecked();
}

bool OwncloudAdvancedSetupPage::validatePage()
{
    if (manualFolderConfig()) {
        return true;
    }

    if (useVirtualFileSync()) {
        const auto availability = Vfs::checkAvailability(localFolder());
        if (!availability) {
            auto msg = new QMessageBox(QMessageBox::Warning, tr("Virtual files are not available for the selected folder"), availability.error(), QMessageBox::Ok, this);
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

        if (_ui.rSyncEverything->isChecked()) {
            ConfigFile cfgFile;
            cfgFile.setNewBigFolderSizeLimit(_ui.confCheckBoxSize->isChecked(),
                _ui.confSpinBox->value());
            cfgFile.setConfirmExternalStorage(_ui.confCheckBoxExternal->isChecked());
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
    if (!err.isEmpty()) {
        auto msg = new QMessageBox(QMessageBox::Warning, tr("Error"), err, QMessageBox::Ok, this);
        msg->setAttribute(Qt::WA_DeleteOnClose);
        msg->open();
    }
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
    if (!dir.isEmpty()) {
        _ui.pbSelectLocalFolder->setText(dir);
        wizard()->setProperty("localFolder", dir);
        updateStatus();
    }
}

void OwncloudAdvancedSetupPage::slotSelectiveSyncClicked()
{
    AccountPtr acc = static_cast<OwncloudWizard *>(wizard())->account();
    SelectiveSyncDialog *dlg = new SelectiveSyncDialog(acc, _remoteFolder, _selectiveSyncBlacklist, this);
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
        } else if (result == QDialog::Rejected && _selectiveSyncBlacklist == QStringList("/")) {
            _selectiveSyncBlacklist = dlg->oldBlackList();
            updateBlacklist = true;
        }

        if (updateBlacklist) {
            if (!_selectiveSyncBlacklist.isEmpty()) {
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
}

void OwncloudAdvancedSetupPage::slotSyncEverythingClicked()
{
    _ui.lSelectiveSyncSizeLabel->setText(QString());
    setRadioChecked(_ui.rSyncEverything);
    _selectiveSyncBlacklist.clear();
}

void OwncloudAdvancedSetupPage::slotQuotaRetrieved(const QMap<QString, QString> &result)
{
    _ui.lSyncEverythingSizeLabel->setText(tr("(%1)").arg(Utility::octetsToString(result["size"].toDouble())));
}

void OwncloudAdvancedSetupPage::setRadioChecked(QRadioButton *radio)
{
    // We don't want clicking the radio buttons to immediately adjust the checked state
    // for selective sync and virtual file sync, so we keep them uncheckable until
    // they should be checked.
    radio->setCheckable(true);
    radio->setChecked(true);

    if (radio != _ui.rVirtualFileSync)
        _ui.rVirtualFileSync->setCheckable(false);

    emit completeChanged();
}

} // namespace OCC
