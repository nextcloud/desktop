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

#include <QAbstractButton>
#include <QtCore>
#include <QProcess>
#include <QMessageBox>
#include <QDesktopServices>

#include "wizard/owncloudwizardcommon.h"
#include "wizard/owncloudwizard.h"
#include "owncloudsetupwizard.h"
#include "configfile.h"
#include "folderman.h"
#include "utility.h"
#include "accessmanager.h"
#include "account.h"
#include "networkjobs.h"
#include "sslerrordialog.h"

#include "creds/credentialsfactory.h"
#include "creds/abstractcredentials.h"
#include "creds/dummycredentials.h"

namespace OCC {

OwncloudSetupWizard::OwncloudSetupWizard(QObject* parent) :
    QObject( parent ),
    _account(0),
    _ocWizard(new OwncloudWizard),
    _remoteFolder()
{
    connect( _ocWizard, SIGNAL(determineAuthType(const QString&)),
             this, SLOT(slotDetermineAuthType(const QString&)));
    connect( _ocWizard, SIGNAL(connectToOCUrl( const QString& ) ),
             this, SLOT(slotConnectToOCUrl( const QString& )));
    connect( _ocWizard, SIGNAL(createLocalAndRemoteFolders(QString, QString)),
             this, SLOT(slotCreateLocalAndRemoteFolders(QString, QString)));
    /* basicSetupFinished might be called from a reply from the network.
       slotAssistantFinished might destroy the temporary QNetworkAccessManager.
       Therefore Qt::QueuedConnection is required */
    connect( _ocWizard, SIGNAL(basicSetupFinished(int)),
             this, SLOT(slotAssistantFinished(int)), Qt::QueuedConnection);
    connect( _ocWizard, SIGNAL(finished(int)), SLOT(deleteLater()));
    connect( _ocWizard, SIGNAL(skipFolderConfiguration()), SLOT(slotSkipFolderConfigruation()));
}

OwncloudSetupWizard::~OwncloudSetupWizard()
{
    _ocWizard->deleteLater();
}

void OwncloudSetupWizard::runWizard(QObject* obj, const char* amember, QWidget *parent)
{
    static QPointer<OwncloudSetupWizard> wiz;

    if (!wiz.isNull()) {
        return;
    }

    wiz = new OwncloudSetupWizard(parent);
    connect( wiz, SIGNAL(ownCloudWizardDone(int)), obj, amember);
    FolderMan::instance()->setSyncEnabled(false);
    wiz->startWizard();
}

void OwncloudSetupWizard::startWizard()
{
    FolderMan *folderMan = FolderMan::instance();
    bool multiFolderSetup = folderMan->map().count() > 1;
    // ###
    Account *account = Account::restore();
    if (!account) {
        _ocWizard->setConfigExists(false);
        account = new Account;
        account->setCredentials(CredentialsFactory::create("dummy"));
    } else {
        _ocWizard->setConfigExists(true);
    }
    account->setSslErrorHandler(new SslDialogErrorHandler);
    _ocWizard->setAccount(account);
    _ocWizard->setOCUrl(account->url().toString());

    _remoteFolder = Theme::instance()->defaultServerFolder();
    // remoteFolder may be empty, which means /
    QString localFolder = Theme::instance()->defaultClientFolder();

    // if its a relative path, prepend with users home dir, otherwise use as absolute path

    if( !QDir(localFolder).isAbsolute() ) {
        localFolder = QDir::homePath() + QDir::separator() + localFolder;
    }

    if (!multiFolderSetup) {
        QList<Folder*> folders = folderMan->map().values();
        if (!folders.isEmpty()) {
            Folder* folder = folders.first();
            localFolder = QDir(folder->path()).absolutePath();
        }
    }

    _ocWizard->setProperty("oldLocalFolder", localFolder);
    _ocWizard->setProperty("localFolder", localFolder);

    // remember the local folder to compare later if it changed, but clean first
    QString lf = QDir::fromNativeSeparators(localFolder);
    if( !lf.endsWith(QLatin1Char('/'))) {
        lf.append(QLatin1Char('/'));
    }

    _initLocalFolder = lf;

    _ocWizard->setRemoteFolder(_remoteFolder);

    _ocWizard->setStartId(WizardCommon::Page_ServerSetup);

    _ocWizard->restart();

    // settings re-initialized in initPage must be set here after restart
    _ocWizard->setMultipleFoldersExist( multiFolderSetup );

    _ocWizard->open();
    _ocWizard->raise();
}

// also checks if an installation is valid and determines auth type in a second step
void OwncloudSetupWizard::slotDetermineAuthType(const QString &urlString)
{
    QString fixedUrl = urlString;
    QUrl url = QUrl::fromUserInput(fixedUrl);
    // fromUserInput defaults to http, not http if no scheme is specified
    if (!fixedUrl.startsWith("http://") && !fixedUrl.startsWith("https://")) {
        url.setScheme("https");
    }
    Account *account = _ocWizard->account();
    account->setUrl(url);
    // Set fake credentials beforfe we check what credential it actually is.
    account->setCredentials(CredentialsFactory::create("dummy"));
    CheckServerJob *job = new CheckServerJob(_ocWizard->account(), this);
    job->setIgnoreCredentialFailure(true);
    connect(job, SIGNAL(instanceFound(QUrl,QVariantMap)), SLOT(slotOwnCloudFoundAuth(QUrl,QVariantMap)));
    connect(job, SIGNAL(instanceNotFound(QNetworkReply*)), SLOT(slotNoOwnCloudFoundAuth(QNetworkReply*)));
    connect(job, SIGNAL(timeout(const QUrl&)), SLOT(slotNoOwnCloudFoundAuthTimeout(const QUrl&)));
    job->setTimeout(10*1000);
    job->start();
}

void OwncloudSetupWizard::slotOwnCloudFoundAuth(const QUrl& url, const QVariantMap &info)
{
    _ocWizard->appendToConfigurationLog(tr("<font color=\"green\">Successfully connected to %1: %2 version %3 (%4)</font><br/><br/>")
                                        .arg(url.toString())
                                        .arg(Theme::instance()->appNameGUI())
                                        .arg(CheckServerJob::versionString(info))
                                        .arg(CheckServerJob::version(info)));

    QString p = url.path();
    if (p.endsWith("/status.php")) {
        // We might be redirected, update the account
        QUrl redirectedUrl = url;
        redirectedUrl.setPath(url.path().left(url.path().length() - 11));
        _ocWizard->account()->setUrl(redirectedUrl);
        qDebug() << Q_FUNC_INFO << " was redirected to" << redirectedUrl.toString();
    }

    DetermineAuthTypeJob *job = new DetermineAuthTypeJob(_ocWizard->account(), this);
    job->setIgnoreCredentialFailure(true);
    connect(job, SIGNAL(authType(WizardCommon::AuthType)),
            _ocWizard, SLOT(setAuthType(WizardCommon::AuthType)));
    job->start();
}

void OwncloudSetupWizard::slotNoOwnCloudFoundAuth(QNetworkReply *reply)
{
    _ocWizard->displayError(tr("Failed to connect to %1 at %2:<br/>%3")
                            .arg(Theme::instance()->appNameGUI(),
                                 reply->url().toString(),
                                 reply->errorString()));
}

void OwncloudSetupWizard::slotNoOwnCloudFoundAuthTimeout(const QUrl&url)
{
    _ocWizard->displayError(tr("Failed to connect to %1 at %2:<br/>%3")
                            .arg(Theme::instance()->appNameGUI(),
                                 url.toString(),
                                 "Timeout"));
}

void OwncloudSetupWizard::slotConnectToOCUrl( const QString& url )
{
    qDebug() << "Connect to url: " << url;
    AbstractCredentials *creds = _ocWizard->getCredentials();
    _ocWizard->account()->setCredentials(creds);
    _ocWizard->setField(QLatin1String("OCUrl"), url );
    _ocWizard->appendToConfigurationLog(tr("Trying to connect to %1 at %2...")
                                        .arg( Theme::instance()->appNameGUI() ).arg(url) );

    testOwnCloudConnect();
}

void OwncloudSetupWizard::testOwnCloudConnect()
{
    Account *account = _ocWizard->account();

    ValidateDavAuthJob *job = new ValidateDavAuthJob(account, this);
    job->setIgnoreCredentialFailure(true);
    connect(job, SIGNAL(authResult(QNetworkReply*)), SLOT(slotConnectionCheck(QNetworkReply*)));
    job->start();
}

void OwncloudSetupWizard::slotConnectionCheck(QNetworkReply* reply)
{
    switch (reply->error()) {
    case QNetworkReply::NoError:
    case QNetworkReply::ContentNotFoundError:
        _ocWizard->successfulStep();
        break;

    default:
        _ocWizard->displayError(tr("Error: Wrong credentials."));
        break;
    }
}

void OwncloudSetupWizard::slotCreateLocalAndRemoteFolders(const QString& localFolder, const QString& remoteFolder)
{
    qDebug() << "Setup local sync folder for new oC connection " << localFolder;
    const QDir fi( localFolder );

    bool nextStep = true;
    if( fi.exists() ) {
        // there is an existing local folder. If its non empty, it can only be synced if the
        // ownCloud is newly created.
        _ocWizard->appendToConfigurationLog( tr("Local sync folder %1 already exists, setting it up for sync.<br/><br/>").arg(localFolder));
    } else {
        QString res = tr("Creating local sync folder %1... ").arg(localFolder);
        if( fi.mkpath( localFolder ) ) {
            Utility::setupFavLink( localFolder );
            // FIXME: Create a local sync folder.
            res += tr("ok");
        } else {
            res += tr("failed.");
            qDebug() << "Failed to create " << fi.path();
            _ocWizard->displayError(tr("Could not create local folder %1").arg(localFolder));
            nextStep = false;
        }
        _ocWizard->appendToConfigurationLog( res );
    }
    if (nextStep) {
        EntityExistsJob *job = new EntityExistsJob(_ocWizard->account(), remoteFolder, this);
        connect(job, SIGNAL(exists(QNetworkReply*)), SLOT(slotAuthCheckReply(QNetworkReply*)));
        job->start();
    } else {
        finalizeSetup( false );
    }
}

// ### TODO move into EntityExistsJob once we decide if/how to return gui strings from jobs
void OwncloudSetupWizard::slotAuthCheckReply(QNetworkReply *reply)
{
    bool ok = true;
    QString error;
    QNetworkReply::NetworkError errId = reply->error();

    if( errId == QNetworkReply::NoError ) {
        qDebug() << "******** Remote folder found, all cool!";
    } else if( errId == QNetworkReply::ContentNotFoundError ) {
        if( _remoteFolder.isEmpty() ) {
            error = tr("No remote folder specified!");
            ok = false;
        } else {
            createRemoteFolder();
        }
    } else {
        error = tr("Error: %1").arg(reply->errorString());
        ok = false;
    }

    if( !ok ) {
        _ocWizard->displayError(error);
    }

    finalizeSetup( ok );
}

void OwncloudSetupWizard::createRemoteFolder()
{
    _ocWizard->appendToConfigurationLog( tr("creating folder on ownCloud: %1" ).arg( _remoteFolder ));

    MkColJob *job = new MkColJob(_ocWizard->account(), _remoteFolder, this);
    connect(job, SIGNAL(finished(QNetworkReply::NetworkError)), SLOT(slotCreateRemoteFolderFinished(QNetworkReply::NetworkError)));
    job->start();
}

void OwncloudSetupWizard::slotCreateRemoteFolderFinished( QNetworkReply::NetworkError error )
{
    qDebug() << "** webdav mkdir request finished " << error;
    //    disconnect(ownCloudInfo::instance(), SIGNAL(webdavColCreated(QNetworkReply::NetworkError)),
    //               this, SLOT(slotCreateRemoteFolderFinished(QNetworkReply::NetworkError)));

    bool success = true;

    if( error == QNetworkReply::NoError ) {
        _ocWizard->appendToConfigurationLog( tr("Remote folder %1 created successfully.").arg(_remoteFolder));
    } else if( error == 202 ) {
        _ocWizard->appendToConfigurationLog( tr("The remote folder %1 already exists. Connecting it for syncing.").arg(_remoteFolder));
    } else if( error > 202 && error < 300 ) {
        _ocWizard->displayError( tr("The folder creation resulted in HTTP error code %1").arg((int)error ));

        _ocWizard->appendToConfigurationLog( tr("The folder creation resulted in HTTP error code %1").arg((int)error) );
    } else if( error == QNetworkReply::OperationCanceledError ) {
        _ocWizard->displayError( tr("The remote folder creation failed because the provided credentials "
                                    "are wrong!"
                                    "<br/>Please go back and check your credentials.</p>"));
        _ocWizard->appendToConfigurationLog( tr("<p><font color=\"red\">Remote folder creation failed probably because the provided credentials are wrong.</font>"
                                                "<br/>Please go back and check your credentials.</p>"));
        _remoteFolder.clear();
        success = false;
    } else {
        _ocWizard->appendToConfigurationLog( tr("Remote folder %1 creation failed with error <tt>%2</tt>.").arg(_remoteFolder).arg(error));
        _ocWizard->displayError( tr("Remote folder %1 creation failed with error <tt>%2</tt>.").arg(_remoteFolder).arg(error) );
        _remoteFolder.clear();
        success = false;
    }

    finalizeSetup( success );
}

void OwncloudSetupWizard::finalizeSetup( bool success )
{
    // enable/disable the finish button.
    _ocWizard->enableFinishOnResultWidget(success);

    const QString localFolder = _ocWizard->property("localFolder").toString();
    if( success ) {
        if( !(localFolder.isEmpty() || _remoteFolder.isEmpty() )) {
            _ocWizard->appendToConfigurationLog( tr("A sync connection from %1 to remote directory %2 was set up.")
                                                 .arg(localFolder).arg(_remoteFolder));
        }
        _ocWizard->appendToConfigurationLog( QLatin1String(" "));
        _ocWizard->appendToConfigurationLog( QLatin1String("<p><font color=\"green\"><b>")
                                             + tr("Successfully connected to %1!")
                                             .arg(Theme::instance()->appNameGUI())
                                             + QLatin1String("</b></font></p>"));
        _ocWizard->successfulStep();
    } else {
        // ### this is not quite true, pass in the real problem as optional parameter
        _ocWizard->appendToConfigurationLog(QLatin1String("<p><font color=\"red\">")
                                            + tr("Connection to %1 could not be established. Please check again.")
                                            .arg(Theme::instance()->appNameGUI())
                                            + QLatin1String("</font></p>"));
    }
}

bool OwncloudSetupWizard::ensureStartFromScratch(const QString &localFolder) {
    // first try to rename (backup) the current local dir.
    bool renameOk = false;
    while( !renameOk ) {
        renameOk = FolderMan::instance()->startFromScratch(localFolder);
        if( ! renameOk ) {
            QMessageBox::StandardButton but;
            but = QMessageBox::question( 0, tr("Folder rename failed"),
                                         tr("Can't remove and back up the folder because the folder or a file in it is open in another program."
                                            " Please close the folder or file and hit retry or cancel the setup."), QMessageBox::Retry | QMessageBox::Abort, QMessageBox::Retry);
            if( but == QMessageBox::Abort ) {
                break;
            }
        }
    }
    return renameOk;
}

void OwncloudSetupWizard::replaceDefaultAccountWith(Account *newAccount)
{
    // new Account
    AccountManager *mgr = AccountManager::instance();
    if (mgr->account()) {
        mgr->account()->deleteLater();
    }
    mgr->setAccount(newAccount);
    newAccount->save();
}

// Method executed when the user ends has finished the basic setup.
void OwncloudSetupWizard::slotAssistantFinished( int result )
{
    FolderMan *folderMan = FolderMan::instance();

    if( result == QDialog::Rejected ) {
        // the old config remains valid. Remove the temporary one.
        _ocWizard->account()->deleteLater();
        qDebug() << "Rejected the new config, use the old!";
    } else if( result == QDialog::Accepted ) {
        Account *newAccount = _ocWizard->account();
        Account *origAccount = AccountManager::instance()->account();

        QString localFolder = QDir::fromNativeSeparators(_ocWizard->localFolder());
        if( !localFolder.endsWith(QLatin1Char('/'))) {
            localFolder.append(QLatin1Char('/'));
        }

        Folder *f = folderMan->folderForPath(localFolder);
        if( f ) {
            folderMan->setSyncEnabled(false);
            f->slotTerminateSync();
            f->journalDb()->close();
        }

        bool isInitialSetup = (origAccount == 0);

        // check if either the account or the local folder changed, than reinit
        bool reinitRequired = _initLocalFolder != localFolder ||
                newAccount->changed(origAccount, true /* ignoreProtocol, allows http->https */);
        bool startFromScratch = _ocWizard->field("OCSyncFromScratch").toBool();

        // This distinguishes three possibilities:
        // 1. Initial setup, no prior account exists
        if (isInitialSetup) {
            folderMan->addFolderDefinition(Theme::instance()->appName(),
                                           localFolder, _remoteFolder, _ocWizard->blacklist() );
            replaceDefaultAccountWith(newAccount);
        }
        // 2. Server URL or user changed, requires reinit of folders
        else if (reinitRequired) {
            folderMan->removeAllFolderDefinitions();
            // 2.1: startFromScratch: (Re)move local data, clean slate sync
            if (startFromScratch) {
                if (ensureStartFromScratch(localFolder)) {
                    folderMan->addFolderDefinition(Theme::instance()->appName(),
                                                   localFolder, _remoteFolder, _ocWizard->blacklist() );
                    _ocWizard->appendToConfigurationLog(tr("<font color=\"green\"><b>Local sync folder %1 successfully created!</b></font>").arg(localFolder));
                    replaceDefaultAccountWith(newAccount);
                }
            }
            // 2.2: Reinit: Remove journal and start a sync
            else {
                folderMan->addFolderDefinition(Theme::instance()->appName(),
                                               localFolder, _remoteFolder, _ocWizard->blacklist() );
                _ocWizard->appendToConfigurationLog(tr("<font color=\"green\"><b>Local sync folder %1 successfully created!</b></font>").arg(localFolder));
                replaceDefaultAccountWith(newAccount);
            }
        }
        // 3. Existing setup, http -> https or password changed
        else {
            replaceDefaultAccountWith(newAccount);
            qDebug() << "Only password was changed, no changes to folder configuration.";
        }
    }

    // notify others.
    emit ownCloudWizardDone( result );
}

void OwncloudSetupWizard::slotSkipFolderConfigruation()
{
    replaceDefaultAccountWith(_ocWizard->account());
    _ocWizard->blockSignals(true);
    _ocWizard->close();
    _ocWizard->blockSignals(false);
    emit ownCloudWizardDone( QDialog::Accepted );
}


DetermineAuthTypeJob::DetermineAuthTypeJob(Account *account, QObject *parent)
    : AbstractNetworkJob(account, QString(), parent)
    , _redirects(0)
{
}

void DetermineAuthTypeJob::start()
{
    QNetworkReply *reply = getRequest(account()->davPath());
    setReply(reply);
    setupConnections(reply);
    AbstractNetworkJob::start();
}

bool DetermineAuthTypeJob::finished()
{
    QUrl redirection = reply()->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
    qDebug() << Q_FUNC_INFO << redirection.toString();
    if (_redirects >= maxRedirects()) {
        redirection.clear();
    }
    if ((reply()->error() == QNetworkReply::AuthenticationRequiredError) || redirection.isEmpty()) {
        emit authType(WizardCommon::HttpCreds);
    } else if (redirection.toString().endsWith(account()->davPath())) {
        // do a new run
        _redirects++;
        setReply(getRequest(redirection));
        setupConnections(reply());
    } else {
        QRegExp shibbolethyWords("SAML|wayf");

        shibbolethyWords.setCaseSensitivity(Qt::CaseInsensitive);
        if (redirection.toString().contains(shibbolethyWords)) {
            emit authType(WizardCommon::Shibboleth);
        } else {
            // TODO: Send an error.
            // eh?
            emit authType(WizardCommon::HttpCreds);
        }
    }
    return true;
}

ValidateDavAuthJob::ValidateDavAuthJob(Account *account, QObject *parent)
    : AbstractNetworkJob(account, QString(), parent)
{
}

void ValidateDavAuthJob::start()
{
    QString p = account()->davPath();
    QNetworkReply *reply = getRequest(p);
    setReply(reply);
    setupConnections(reply);
    AbstractNetworkJob::start();
}

bool ValidateDavAuthJob::finished()
{
    emit authResult(reply());
    return true;
}

} // namespace OCC
