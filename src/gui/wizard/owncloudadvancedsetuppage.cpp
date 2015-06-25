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
#include "creds/abstractcredentials.h"
#include "networkjobs.h"

namespace OCC
{

OwncloudAdvancedSetupPage::OwncloudAdvancedSetupPage()
  : QWizardPage(),
    _ui(),
    _checking(false),
    _created(false),
    _configExists(false),
    _multipleFoldersExist(false),
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
    return !_checking;
}

void OwncloudAdvancedSetupPage::initializePage()
{
    WizardCommon::initErrorLabel(_ui.errorLabel);

    _checking  = false;
    _multipleFoldersExist = false;
    _oldLocalFolder = wizard()->property("oldLocalFolder").toString();
    _ui.lSelectiveSyncSizeLabel->setText(QString());
    _ui.lSyncEverythingSizeLabel->setText(QString());

    // call to init label
    updateStatus();

    // ensure "next" gets the focus, not obSelectLocalFolder
    QTimer::singleShot(0, wizard()->button(QWizard::NextButton), SLOT(setFocus()));

    auto acc = static_cast<OwncloudWizard *>(wizard())->account();
    auto quotaJob = new PropfindJob(acc, _remoteFolder, this);
    quotaJob->setProperties(QList<QByteArray>() << "quota-used-bytes");

    connect(quotaJob, SIGNAL(result(QVariantMap)), SLOT(slotQuotaRetrieved(QVariantMap)));
    quotaJob->start();


    if (Theme::instance()->wizardSelectiveSyncDefaultNothing()) {
        _selectiveSyncBlacklist = QStringList("/");
        QTimer::singleShot(0, this, SLOT(slotSelectiveSyncClicked()));
    }
}

// Called if the user changes the user- or url field. Adjust the texts and
// evtl. warnings on the dialog.
void OwncloudAdvancedSetupPage::updateStatus()
{
    const QString locFolder = localFolder();
    // check if the local folder exists. If so, and if its not empty, show a warning.
    QString t;

    _ui.pbSelectLocalFolder->setText(QDir::toNativeSeparators(locFolder));
    if (dataChanged()) {
        if( _remoteFolder.isEmpty() || _remoteFolder == QLatin1String("/") ) {
            t = "";
        } else {
            t = tr("%1 folder '%2' is synced to local folder '%3'")
                .arg(Theme::instance()->appName()).arg(_remoteFolder)
                .arg(QDir::toNativeSeparators(locFolder));
              _ui.rSyncEverything->setText(tr("Sync the directory '%1'").arg(_remoteFolder));
        }

        if ( _multipleFoldersExist ) {
            t += tr("<p><small><strong>Warning:</strong> You currently have multiple folders "
                    "configured. If you continue with the current settings, the folder configurations "
                    "will be discarded and a single root folder sync will be created!</small></p>");
        }

        const bool dirNotEmpty(QDir(locFolder).entryList(QDir::AllEntries | QDir::NoDotAndDotDot).count() > 0);
        if(dirNotEmpty) {
            t += tr("<p><small><strong>Warning:</strong> The local directory is not empty. "
                    "Pick a resolution!</small></p>");
        }
        _ui.resolutionWidget->setVisible(dirNotEmpty);
    } else {
        _ui.resolutionWidget->setVisible(false);
    }

    _ui.syncModeLabel->setText(t);
    _ui.syncModeLabel->setFixedHeight(_ui.syncModeLabel->sizeHint().height());
    wizard()->resize(wizard()->sizeHint());
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

bool OwncloudAdvancedSetupPage::validatePage()
{
    if(!_created) {
        setErrorString(QString::null);
        _checking = true;
        startSpinner();
        emit completeChanged();

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

void OwncloudAdvancedSetupPage::setMultipleFoldersExist(bool exist)
{
    _multipleFoldersExist = exist;
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

void OwncloudAdvancedSetupPage::setConfigExists(bool config)
{
    _configExists = config;

    if (config == true) {
        setSubTitle(WizardCommon::subTitleTemplate().arg(tr("Update advanced setup")));
    }
}

void OwncloudAdvancedSetupPage::slotSelectiveSyncClicked()
{
    // Because clicking on it also changes it, restore it to the previous state in case the user cancel the dialog
    _ui.rSyncEverything->setChecked(_selectiveSyncBlacklist.isEmpty());

    AccountPtr acc = static_cast<OwncloudWizard *>(wizard())->account();
    SelectiveSyncDialog *dlg = new SelectiveSyncDialog(acc, _remoteFolder, _selectiveSyncBlacklist, this);
    if (dlg->exec() == QDialog::Accepted) {
        _selectiveSyncBlacklist = dlg->createBlackList();
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
    _ui.lSyncEverythingSizeLabel->setText(tr("(%1)").arg(Utility::octetsToString(result["quota-used-bytes"].toDouble())));

}

} // namespace OCC
