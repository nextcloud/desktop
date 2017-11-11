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
#include <QPushButton>
#include <QMessageBox>
#include <QSsl>
#include <QSslCertificate>
#include <QNetworkAccessManager>

#include "QProgressIndicator.h"

#include "wizard/owncloudwizardcommon.h"
#include "wizard/owncloudsetuppage.h"
#include "wizard/owncloudconnectionmethoddialog.h"
#include "theme.h"
#include "account.h"

namespace OCC {

OwncloudSetupPage::OwncloudSetupPage(QWidget *parent)
    : QWizardPage()
    , _ui()
    , _oCUrl()
    , _ocUser()
    , _authTypeKnown(false)
    , _checking(false)
    , _authType(DetermineAuthTypeJob::Basic)
    , _progressIndi(new QProgressIndicator(this))
{
    _ui.setupUi(this);
    _ocWizard = qobject_cast<OwncloudWizard *>(parent);

    Theme *theme = Theme::instance();
    setTitle(WizardCommon::titleTemplate().arg(tr("Connect to %1").arg(theme->appNameGUI())));
    setSubTitle(WizardCommon::subTitleTemplate().arg(tr("Setup %1 server").arg(theme->appNameGUI())));

    if (theme->overrideServerUrl().isEmpty()) {
        _ui.leUrl->setPostfix(theme->wizardUrlPostfix());
        _ui.leUrl->setPlaceholderText(theme->wizardUrlHint());
    } else {
        _ui.leUrl->setEnabled(false);
    }


    registerField(QLatin1String("OCUrl*"), _ui.leUrl);

    _ui.resultLayout->addWidget(_progressIndi);
    stopSpinner();

    setupCustomization();

    slotUrlChanged(QLatin1String("")); // don't jitter UI
    connect(_ui.leUrl, &QLineEdit::textChanged, this, &OwncloudSetupPage::slotUrlChanged);
    connect(_ui.leUrl, &QLineEdit::editingFinished, this, &OwncloudSetupPage::slotUrlEditFinished);

    addCertDial = new AddCertificateDialog(this);
}

void OwncloudSetupPage::setServerUrl(const QString &newUrl)
{
    _oCUrl = newUrl;
    if (_oCUrl.isEmpty()) {
        _ui.leUrl->clear();
        return;
    }

    _ui.leUrl->setText(_oCUrl);
}

void OwncloudSetupPage::setupCustomization()
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
}

// slot hit from textChanged of the url entry field.
void OwncloudSetupPage::slotUrlChanged(const QString &url)
{
    _authTypeKnown = false;

    QString newUrl = url;
    if (url.endsWith("index.php")) {
        newUrl.chop(9);
    }
    if (_ocWizard && _ocWizard->account()) {
        QString webDavPath = _ocWizard->account()->davPath();
        if (url.endsWith(webDavPath)) {
            newUrl.chop(webDavPath.length());
        }
        if (webDavPath.endsWith(QLatin1Char('/'))) {
            webDavPath.chop(1); // cut off the slash
            if (url.endsWith(webDavPath)) {
                newUrl.chop(webDavPath.length());
            }
        }
    }
    if (newUrl != url) {
        _ui.leUrl->setText(newUrl);
    }

    if (!url.startsWith(QLatin1String("https://"))) {
        _ui.urlLabel->setPixmap(QPixmap(Theme::hidpiFileName(":/client/resources/lock-http.png")));
        _ui.urlLabel->setToolTip(tr("This url is NOT secure as it is not encrypted.\n"
                                    "It is not advisable to use it."));
    } else {
        _ui.urlLabel->setPixmap(QPixmap(Theme::hidpiFileName(":/client/resources/lock-https.png")));
        _ui.urlLabel->setToolTip(tr("This url is secure. You can use it."));
    }
}

void OwncloudSetupPage::slotUrlEditFinished()
{
    QString url = _ui.leUrl->fullText();
    if (QUrl(url).isRelative() && !url.isEmpty()) {
        // no scheme defined, set one
        url.prepend("https://");
    }
    _ui.leUrl->setFullText(url);
}

bool OwncloudSetupPage::isComplete() const
{
    return !_ui.leUrl->text().isEmpty() && !_checking;
}

void OwncloudSetupPage::initializePage()
{
    WizardCommon::initErrorLabel(_ui.errorLabel);

    _authTypeKnown = false;
    _checking = false;

    QAbstractButton *nextButton = wizard()->button(QWizard::NextButton);
    QPushButton *pushButton = qobject_cast<QPushButton *>(nextButton);
    if (pushButton)
        pushButton->setDefault(true);

    // If url is overriden by theme, it's already set and
    // we just check the server type and switch to second page
    // immediately.
    if (Theme::instance()->overrideServerUrl().isEmpty()) {
        _ui.leUrl->setFocus();
    } else {
        setCommitPage(true);
        // Hack: setCommitPage() changes caption, but after an error this page could still be visible
        setButtonText(QWizard::CommitButton, tr("&Next >"));
        validatePage();
        setVisible(false);
    }
}

bool OwncloudSetupPage::urlHasChanged()
{
    bool change = false;
    const QChar slash('/');

    QUrl currentUrl(url());
    QUrl initialUrl(_oCUrl);

    QString currentPath = currentUrl.path();
    QString initialPath = initialUrl.path();

    // add a trailing slash.
    if (!currentPath.endsWith(slash))
        currentPath += slash;
    if (!initialPath.endsWith(slash))
        initialPath += slash;

    if (currentUrl.host() != initialUrl.host() || currentUrl.port() != initialUrl.port() || currentPath != initialPath) {
        change = true;
    }

    return change;
}

int OwncloudSetupPage::nextId() const
{
    if (_authType == DetermineAuthTypeJob::Basic) {
        return WizardCommon::Page_HttpCreds;
    } else if (_authType == DetermineAuthTypeJob::OAuth) {
        return WizardCommon::Page_OAuthCreds;
    } else {
        return WizardCommon::Page_ShibbolethCreds;
    }
}

QString OwncloudSetupPage::url() const
{
    QString url = _ui.leUrl->fullText().simplified();
    return url;
}

bool OwncloudSetupPage::validatePage()
{
    if (!_authTypeKnown) {
        setErrorString(QString::null, false);
        _checking = true;
        startSpinner();
        emit completeChanged();

        emit determineAuthType(url());
        return false;
    } else {
        // connecting is running
        stopSpinner();
        _checking = false;
        emit completeChanged();
        return true;
    }
}

void OwncloudSetupPage::setAuthType(DetermineAuthTypeJob::AuthType type)
{
    _authTypeKnown = true;
    _authType = type;
    stopSpinner();
}

void OwncloudSetupPage::setErrorString(const QString &err, bool retryHTTPonly)
{
    if (err.isEmpty()) {
        _ui.errorLabel->setVisible(false);
    } else {
        if (retryHTTPonly) {
            QUrl url(_ui.leUrl->fullText());
            if (url.scheme() == "https") {
                // Ask the user how to proceed when connecting to a https:// URL fails.
                // It is possible that the server is secured with client-side TLS certificates,
                // but that it has no way of informing the owncloud client that this is the case.

                OwncloudConnectionMethodDialog dialog;
                dialog.setUrl(url);
                // FIXME: Synchronous dialogs are not so nice because of event loop recursion
                int retVal = dialog.exec();

                switch (retVal) {
                case OwncloudConnectionMethodDialog::No_TLS: {
                    url.setScheme("http");
                    _ui.leUrl->setFullText(url.toString());
                    // skip ahead to next page, since the user would expect us to retry automatically
                    wizard()->next();
                } break;
                case OwncloudConnectionMethodDialog::Client_Side_TLS:
                    addCertDial->show();
                    connect(addCertDial, &QDialog::accepted, this, &OwncloudSetupPage::slotCertificateAccepted);
                    break;
                case OwncloudConnectionMethodDialog::Closed:
                case OwncloudConnectionMethodDialog::Back:
                default:
                    // No-op.
                    break;
                }
            }
        }

        _ui.errorLabel->setVisible(true);
        _ui.errorLabel->setText(err);
    }
    _checking = false;
    emit completeChanged();
    stopSpinner();
}

void OwncloudSetupPage::startSpinner()
{
    _ui.resultLayout->setEnabled(true);
    _progressIndi->setVisible(true);
    _progressIndi->startAnimation();
}

void OwncloudSetupPage::stopSpinner()
{
    _ui.resultLayout->setEnabled(false);
    _progressIndi->setVisible(false);
    _progressIndi->stopAnimation();
}

QString subjectInfoHelper(const QSslCertificate &cert, const QByteArray &qa)
{
    return cert.subjectInfo(qa).join(QLatin1Char('/'));
}

//called during the validation of the client certificate.
void OwncloudSetupPage::slotCertificateAccepted()
{
    QList<QSslCertificate> clientCaCertificates;
    QFile certFile(addCertDial->getCertificatePath());
    certFile.open(QFile::ReadOnly);
    if (QSslCertificate::importPkcs12(&certFile,
            &_ocWizard->_clientSslKey, &_ocWizard->_clientSslCertificate,
            &clientCaCertificates,
            addCertDial->getCertificatePasswd().toLocal8Bit())) {
        AccountPtr acc = _ocWizard->account();

        // to re-create the session ticket because we added a key/cert
        acc->setSslConfiguration(QSslConfiguration());
        QSslConfiguration sslConfiguration = acc->getOrCreateSslConfig();

        // We're stuffing the certificate into the configuration form here. Later the
        // cert will come via the HttpCredentials
        sslConfiguration.setLocalCertificate(_ocWizard->_clientSslCertificate);
        sslConfiguration.setPrivateKey(_ocWizard->_clientSslKey);
        acc->setSslConfiguration(sslConfiguration);

        // Make sure TCP connections get re-established
        acc->networkAccessManager()->clearAccessCache();

        addCertDial->reinit(); // FIXME: Why not just have this only created on use?
        validatePage();
    } else {
        addCertDial->showErrorMessage(tr("Could not load certificate. Maybe wrong password?"));
        addCertDial->show();
    }
}

OwncloudSetupPage::~OwncloudSetupPage()
{
}

} // namespace OCC
