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
#include "networkjobs.h"

namespace OCC
{

OwncloudAdvancedSetupPage::OwncloudAdvancedSetupPage()
  : QWizardPage(),
    _ui(),
    _checking(false),
    _created(false),
    _localFolderValid(false),
    _progressIndi(new QProgressIndicator (this)),
    _oldLocalFolder(),
    _remoteFolder()
{
    _ui.setupUi(this);

    Theme *theme = Theme::instance();
    setTitle(WizardCommon::titleTemplate().arg(tr("Connect to %1").arg(theme->appNameGUI())));
    setSubTitle(WizardCommon::subTitleTemplate().arg(tr("Setup local folder options")));

    registerField( QLatin1String("OCSyncFromScratch"), _ui.cbSyncFromScratch);

    _ui.resultLayout->addWidget( _progressIndi );
    stopSpinner();
    setupCustomization();

    connect( _ui.pbSelectLocalFolder, SIGNAL(clicked()), SLOT(slotSelectFolder()));
    setButtonText(QWizard::NextButton, tr("Connect..."));

    connect( _ui.rSyncEverything, SIGNAL(clicked()), SLOT(slotSyncEverythingClicked()));
    connect( _ui.rSelectiveSync, SIGNAL(clicked()), SLOT(slotSelectiveSyncClicked()));
    connect( _ui.bSelectiveSync, SIGNAL(clicked()), SLOT(slotSelectiveSyncClicked()));

    QIcon appIcon = theme->applicationIcon();
    _ui.lServerIcon->setText(QString());
    _ui.lServerIcon->setPixmap(appIcon.pixmap(48));
    _ui.lLocalIcon->setText(QString());
    _ui.lLocalIcon->setPixmap(QPixmap(Theme::hidpiFileName(":/client/resources/folder-sync.png")));

    if (theme->wizardHideExternalStorageConfirmationCheckbox()) {
        _ui.confCheckBoxExternal->hide();
    }
    if (theme->wizardHideFolderSizeLimitCheckbox()) {
        _ui.confCheckBoxSize->hide();
        _ui.confSpinBox->hide();
        _ui.confTraillingSizeLabel->hide();
    }
}

void OwncloudAdvancedSetupPage::setupCustomization()
{
    // set defaults for the customize labels.
    _ui.topLabel->hide();
    _ui.bottomLabel->hide();

    Theme *theme = Theme::instance();
    QVariant variant = theme->customMedia( Theme::oCSetupTop );
    if( !variant.isNull() ) {
        WizardCommon::setupCustomMedia( variant, _ui.topLabel );
    }

    variant = theme->customMedia( Theme::oCSetupBottom );
    WizardCommon::setupCustomMedia( variant, _ui.bottomLabel );
}

bool OwncloudAdvancedSetupPage::isComplete() const
{
    return !_checking && _localFolderValid;
}

void OwncloudAdvancedSetupPage::initializePage()
{
    WizardCommon::initErrorLabel(_ui.errorLabel);

    _checking  = false;
    _oldLocalFolder = wizard()->property("oldLocalFolder").toString();
    _ui.lSelectiveSyncSizeLabel->setText(QString());
    _ui.lSyncEverythingSizeLabel->setText(QString());

    // call to init label
    updateStatus();

    // ensure "next" gets the focus, not obSelectLocalFolder
    QTimer::singleShot(0, wizard()->button(QWizard::NextButton), SLOT(setFocus()));

    auto acc = static_cast<OwncloudWizard *>(wizard())->account();
    auto quotaJob = new PropfindJob(acc, _remoteFolder, this);
    quotaJob->setProperties(QList<QByteArray>() << "http://owncloud.org/ns:size");

    connect(quotaJob, SIGNAL(result(QVariantMap)), SLOT(slotQuotaRetrieved(QVariantMap)));
    quotaJob->start();


    if (Theme::instance()->wizardSelectiveSyncDefaultNothing()) {
        _selectiveSyncBlacklist = QStringList("/");
        QTimer::singleShot(0, this, SLOT(slotSelectiveSyncClicked()));
    }

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
    const QString url = static_cast<OwncloudWizard *>(wizard())->ocUrl();
    const QString user = static_cast<OwncloudWizard *>(wizard())->getCredentials()->user();

    QUrl serverUrl(url);
    serverUrl.setUserName(user);
    // check if the local folder exists. If so, and if its not empty, show a warning.
    QString errorStr = FolderMan::instance()->checkPathValidityForNewFolder(locFolder, serverUrl);
    _localFolderValid = errorStr.isEmpty();

    QString t;

    _ui.pbSelectLocalFolder->setText(QDir::toNativeSeparators(locFolder));
    if (dataChanged()) {
        if( _remoteFolder.isEmpty() || _remoteFolder == QLatin1String("/") ) {
            t = "";
        } else {
            t = tr("%1 folder '%2' is synced to local folder '%3'")
                .arg(Theme::instance()->appName()).arg(_remoteFolder)
                .arg(QDir::toNativeSeparators(locFolder));
              _ui.rSyncEverything->setText(tr("Sync the folder '%1'").arg(_remoteFolder));
        }

        const bool dirNotEmpty(QDir(locFolder).entryList(QDir::AllEntries | QDir::NoDotAndDotDot).count() > 0);
        if(dirNotEmpty) {
            t += tr("<p><small><strong>Warning:</strong> The local folder is not empty. "
                    "Pick a resolution!</small></p>");
        }
        _ui.resolutionWidget->setVisible(dirNotEmpty);
    } else {
        _ui.resolutionWidget->setVisible(false);
    }

    _ui.syncModeLabel->setText(t);
    _ui.syncModeLabel->setFixedHeight(_ui.syncModeLabel->sizeHint().height());
    wizard()->resize(wizard()->sizeHint());
    setErrorString(errorStr);
    emit completeChanged();
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

bool OwncloudAdvancedSetupPage::isConfirmBigFolderChecked() const
{
    return _ui.rSyncEverything->isChecked() && _ui.confCheckBoxSize->isChecked();
}

bool OwncloudAdvancedSetupPage::validatePage()
{
    if(!_created) {
        setErrorString(QString::null);
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

void OwncloudAdvancedSetupPage::setErrorString( const QString& err )
{
    if( err.isEmpty()) {
        _ui.errorLabel->setVisible(false);
    } else {
        _ui.errorLabel->setVisible(true);
        _ui.errorLabel->setText(err);
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

void OwncloudAdvancedSetupPage::setRemoteFolder( const QString& remoteFolder )
{
    if( !remoteFolder.isEmpty() ) {
        _remoteFolder = remoteFolder;
    }
}

void OwncloudAdvancedSetupPage::slotSelectFolder()
{
    QString dir = QFileDialog::getExistingDirectory(0, tr("Local Sync Folder"), QDir::homePath());
    if( !dir.isEmpty() ) {
        _ui.pbSelectLocalFolder->setText(dir);
        wizard()->setProperty("localFolder", dir);
        updateStatus();
    }
}

void OwncloudAdvancedSetupPage::slotSelectiveSyncClicked()
{
    // Because clicking on it also changes it, restore it to the previous state in case the user cancelled the dialog
    _ui.rSyncEverything->setChecked(_selectiveSyncBlacklist.isEmpty());

    AccountPtr acc = static_cast<OwncloudWizard *>(wizard())->account();
    SelectiveSyncDialog *dlg = new SelectiveSyncDialog(acc, _remoteFolder, _selectiveSyncBlacklist, this);

    const int result = dlg->exec();
    bool updateBlacklist = false;

    // We need to update the selective sync blacklist either when the dialog
    // was accepted, or when it was used in conjunction with the
    // wizardSelectiveSyncDefaultNothing feature and was cancelled - in that
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
            _ui.rSelectiveSync->blockSignals(true);
            _ui.rSelectiveSync->setChecked(true);
            _ui.rSelectiveSync->blockSignals(false);
            auto s = dlg->estimatedSize();
            if (s > 0 ) {
                _ui.lSelectiveSyncSizeLabel->setText(tr("(%1)").arg(Utility::octetsToString(s)));
            } else {
                _ui.lSelectiveSyncSizeLabel->setText(QString());
            }
        } else {
            _ui.rSyncEverything->setChecked(true);
            _ui.lSelectiveSyncSizeLabel->setText(QString());
        }
        wizard()->setProperty("blacklist", _selectiveSyncBlacklist);
    }
}

void OwncloudAdvancedSetupPage::slotSyncEverythingClicked()
{
    _ui.lSelectiveSyncSizeLabel->setText(QString());
    _ui.rSyncEverything->setChecked(true);
    _selectiveSyncBlacklist.clear();
}

void OwncloudAdvancedSetupPage::slotQuotaRetrieved(const QVariantMap &result)
{
    _ui.lSyncEverythingSizeLabel->setText(tr("(%1)").arg(Utility::octetsToString(result["size"].toDouble())));

}

} // namespace OCC

