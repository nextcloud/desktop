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

#include "account.h"
#include "config.h"
#include "configfile.h"
#include "theme.h"

#include "application.h"
#include "settingsdialog.h"

#include "wizard/owncloudwizard.h"
#include "wizard/owncloudsetuppage.h"
#include "wizard/owncloudhttpcredspage.h"
#include "wizard/owncloudoauthcredspage.h"
#include "wizard/owncloudadvancedsetuppage.h"

#include "common/vfs.h"

#include "QProgressIndicator.h"

#include <QtCore>
#include <QtGui>
#include <QMessageBox>
#include <owncloudgui.h>

#include <stdlib.h>

namespace {
auto initLocalFolder()
{
    auto localFolder = OCC::Theme::instance()->defaultClientFolder();
    // Update the local folder - this is not guaranteed to find a good one
    // if its a relative path, prepend with users home dir, otherwise use as absolute path

    if (!QDir(localFolder).isAbsolute()) {
        localFolder = QDir::homePath() + QDir::separator() + localFolder;
    }
    return OCC::FolderMan::instance()->findGoodPathForNewSyncFolder(localFolder);
}
}
namespace OCC {

Q_LOGGING_CATEGORY(lcWizard, "gui.wizard", QtInfoMsg)

OwncloudWizard::OwncloudWizard(QWidget *parent)
    : QWizard(parent)
    , _remoteFolder(Theme::instance()->defaultServerFolder())
    , _localFolder(initLocalFolder())
    , _account(nullptr)
    , _setupPage(new OwncloudSetupPage(this))
    , _httpCredsPage(new OwncloudHttpCredsPage(this))
    , _oauthCredsPage(new OwncloudOAuthCredsPage)
    , _advancedSetupPage(new OwncloudAdvancedSetupPage)
    , _credentialsPage(nullptr)
{
    setObjectName("owncloudWizard");

    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setPage(WizardCommon::Page_ServerSetup, _setupPage);
    setPage(WizardCommon::Page_HttpCreds, _httpCredsPage);
    setPage(WizardCommon::Page_OAuthCreds, _oauthCredsPage);
    setPage(WizardCommon::Page_AdvancedSetup, _advancedSetupPage);

    connect(this, &QDialog::finished, this, &OwncloudWizard::basicSetupFinished);

    // note: start Id is set by the calling class depending on if the
    // welcome text is to be shown or not.
    setWizardStyle(QWizard::ModernStyle);

    connect(this, &QWizard::currentIdChanged, this, &OwncloudWizard::slotCurrentPageChanged);
    connect(_setupPage, &OwncloudSetupPage::determineAuthType, this, &OwncloudWizard::determineAuthType);
    connect(_httpCredsPage, &OwncloudHttpCredsPage::connectToOCUrl, this, &OwncloudWizard::connectToOCUrl);
    connect(_oauthCredsPage, &OwncloudOAuthCredsPage::connectToOCUrl, this, &OwncloudWizard::connectToOCUrl);

    Theme *theme = Theme::instance();
    setWindowTitle(tr("%1 Connection Wizard").arg(theme->appNameGUI()));
    setWizardStyle(QWizard::ModernStyle);
    setPixmap(QWizard::BannerPixmap, theme->wizardHeaderBanner({ width(), 78 }));
    setPixmap(QWizard::LogoPixmap, theme->wizardHeaderLogo().pixmap(132, 63));
    setOption(QWizard::NoBackButtonOnStartPage);
    setOption(QWizard::NoBackButtonOnLastPage);
    setOption(QWizard::NoCancelButton, false);
    setOption(QWizard::CancelButtonOnLeft);
    setTitleFormat(Qt::RichText);
    setSubTitleFormat(Qt::RichText);

    setMinimumSize(minimumSizeHint());
}

void OwncloudWizard::setAccount(AccountPtr account)
{
    _account = account;
}

AccountPtr OwncloudWizard::account() const
{
    return _account;
}

const QString &OwncloudWizard::localFolder() const
{
    return _localFolder;
}

void OwncloudWizard::setLocalFolder(const QString &newLocalFolder)
{
    _localFolder = newLocalFolder;
}

QStringList OwncloudWizard::selectiveSyncBlacklist() const
{
    return _advancedSetupPage->selectiveSyncBlacklist();
}

bool OwncloudWizard::useVirtualFileSync() const
{
    return _advancedSetupPage->useVirtualFileSync();
}

bool OwncloudWizard::manualFolderConfig() const
{
    return _advancedSetupPage->manualFolderConfig();
}

bool OwncloudWizard::isConfirmBigFolderChecked() const
{
    return _advancedSetupPage->isConfirmBigFolderChecked();
}

QString OwncloudWizard::ocUrl() const
{
    QString url = field("OCUrl").toString().simplified();
    return url;
}

void OwncloudWizard::setRemoteFolder(const QString &remoteFolder)
{
    _remoteFolder = remoteFolder;
}

void OwncloudWizard::successfulStep()
{
    const int id(currentId());

    switch (id) {
    case WizardCommon::Page_HttpCreds:
        _httpCredsPage->setConnected();
        break;

    case WizardCommon::Page_OAuthCreds:
        _oauthCredsPage->setConnected();
        break;

    case WizardCommon::Page_AdvancedSetup:
        _advancedSetupPage->directoriesCreated();
        break;

    case WizardCommon::Page_ServerSetup:
        qCWarning(lcWizard, "Should not happen at this stage.");
        break;
    }

    ownCloudGui::raiseDialog(this);
    if (nextId() == -1) {
        disconnect(this, &QDialog::finished, this, &OwncloudWizard::basicSetupFinished);
        emit basicSetupFinished(QDialog::Accepted);
    } else {
        next();
    }
}

const QString &OwncloudWizard::remoteFolder() const
{
    return _remoteFolder;
}

void OwncloudWizard::resetRemoteFolder()
{
    _remoteFolder = Theme::instance()->defaultServerFolder();
}

DetermineAuthTypeJob::AuthType OwncloudWizard::authType() const
{
    return _authType;
}

void OwncloudWizard::setAuthType(DetermineAuthTypeJob::AuthType type)
{
    _authType = type;
    _setupPage->setAuthType();
    if (type == DetermineAuthTypeJob::AuthType::OAuth) {
        _credentialsPage = _oauthCredsPage;
    } else { // try Basic auth even for "Unknown"
        _credentialsPage = _httpCredsPage;
    }
    next();
}

// TODO: update this function
void OwncloudWizard::slotCurrentPageChanged(int id)
{
    qCDebug(lcWizard) << "Current Wizard page changed to " << id;

    if (id == WizardCommon::Page_ServerSetup) {
        emit clearPendingRequests();
    }
}

void OwncloudWizard::displayError(const QString &msg)
{
    switch (currentId()) {
    case WizardCommon::Page_ServerSetup:
        _setupPage->setErrorString(msg);
        break;

    case WizardCommon::Page_HttpCreds:
        _httpCredsPage->setErrorString(msg);
        break;

    case WizardCommon::Page_AdvancedSetup:
        _advancedSetupPage->setErrorString(msg);
        break;
    }
}

void OwncloudWizard::setOCUrl(const QString &url)
{
    _setupPage->setServerUrl(url);
}

AbstractCredentials *OwncloudWizard::getCredentials() const
{
    if (_credentialsPage) {
        return _credentialsPage->getCredentials();
    }

    return nullptr;
}

void OwncloudWizard::askExperimentalVirtualFilesFeature(QWidget *receiver, const std::function<void(bool enable)> &callback)
{
    const auto bestVfsMode = bestAvailableVfsMode();
    QMessageBox *msgBox = nullptr;
    QPushButton *acceptButton = nullptr;
    switch (bestVfsMode)
    {
    case Vfs::WindowsCfApi:
        callback(true);
        return;
    case Vfs::WithSuffix:
        msgBox = new QMessageBox(
            QMessageBox::Warning,
            tr("Enable experimental feature?"),
            tr("When the \"virtual files\" mode is enabled no files will be downloaded initially. "
               "Instead, a tiny \"%1\" file will be created for each file that exists on the server. "
               "The contents can be downloaded by running these files or by using their context menu."
               "\n\n"
               "The virtual files mode is mutually exclusive with selective sync. "
               "Currently unselected folders will be translated to online-only folders "
               "and your selective sync settings will be reset."
               "\n\n"
               "Switching to this mode will abort any currently running synchronization."
               "\n\n"
               "This is a new, experimental mode. If you decide to use it, please report any "
               "issues that come up.")
                .arg(APPLICATION_DOTVIRTUALFILE_SUFFIX), QMessageBox::NoButton, receiver);
        acceptButton = msgBox->addButton(tr("Enable experimental placeholder mode"), QMessageBox::AcceptRole);
        msgBox->addButton(tr("Stay safe"), QMessageBox::RejectRole);
        break;
    case Vfs::Off:
        Q_UNREACHABLE();
    }

    connect(msgBox, &QMessageBox::accepted, receiver, [callback, msgBox, acceptButton] {
        callback(msgBox->clickedButton() == acceptButton);
        msgBox->deleteLater();
    });
    msgBox->open();
}

QSize OCC::OwncloudWizard::minimumSizeHint() const
{
    return ocApp()->gui()->settingsDialog()->sizeHintForChild();
}

} // end namespace
