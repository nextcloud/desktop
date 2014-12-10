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

#include "QProgressIndicator.h"

#include "wizard/owncloudwizardcommon.h"
#include "wizard/owncloudsetuppage.h"
#include "theme.h"

namespace OCC
{

OwncloudSetupPage::OwncloudSetupPage()
  : QWizardPage(),
    _ui(),
    _oCUrl(),
    _ocUser(),
    _authTypeKnown(false),
    _checking(false),
    _authType(WizardCommon::HttpCreds),
    _progressIndi(new QProgressIndicator (this))
{
    _ui.setupUi(this);

    Theme *theme = Theme::instance();
    setTitle(WizardCommon::titleTemplate().arg(tr("Connect to %1").arg(theme->appNameGUI())));
    setSubTitle(WizardCommon::subTitleTemplate().arg(tr("Setup %1 server").arg(theme->appNameGUI())));

    registerField( QLatin1String("OCUrl*"), _ui.leUrl );

    _ui.resultLayout->addWidget( _progressIndi );
    stopSpinner();

    setupCustomization();

    connect(_ui.leUrl, SIGNAL(textChanged(QString)), SLOT(slotUrlChanged(QString)));
    connect(_ui.leUrl, SIGNAL(editingFinished()), SLOT(slotUrlEditFinished()));
}

void OwncloudSetupPage::setServerUrl( const QString& newUrl )
{
    _oCUrl = newUrl;
    if( _oCUrl.isEmpty() ) {
        _ui.leUrl->clear();
        return;
    }

    _ui.leUrl->setText( _oCUrl );
}

void OwncloudSetupPage::setupCustomization()
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

// slot hit from textChanged of the url entry field.
void OwncloudSetupPage::slotUrlChanged(const QString& url)
{

    QString newUrl = url;
    if (url.endsWith("index.php")) {
        newUrl.chop(9);
    }
    if (url.endsWith("remote.php/webdav")) {
        newUrl.chop(17);
    }
    if (url.endsWith("remote.php/webdav/")) {
        newUrl.chop(18);
    }
    if (newUrl != url) {
        _ui.leUrl->setText(newUrl);
    }

    if (url.startsWith(QLatin1String("http://"))) {
        _ui.urlLabel->setPixmap( QPixmap(":/client/resources/lock-http.png"));
        _ui.urlLabel->setToolTip(tr("This url is NOT secure as it is not encrypted.\n"
                                    "It is not advisable to use it."));
    } else {
        _ui.urlLabel->setPixmap( QPixmap(":/client/resources/lock-https.png"));
        _ui.urlLabel->setToolTip(tr("This url is secure. You can use it."));
    }
}

void OwncloudSetupPage::slotUrlEditFinished()
{
    QString url = _ui.leUrl->text();
    if (QUrl(url).isRelative()) {
        // no scheme defined, set one
        url.prepend("https://");
    }
    _ui.leUrl->setText(url);
}

bool OwncloudSetupPage::isComplete() const
{
    return !_ui.leUrl->text().isEmpty() && !_checking;
}

void OwncloudSetupPage::initializePage()
{
    WizardCommon::initErrorLabel(_ui.errorLabel);

    _authTypeKnown = false;
    _checking  = false;

    QAbstractButton *nextButton = wizard()->button(QWizard::NextButton);
    QPushButton *pushButton = qobject_cast<QPushButton*>(nextButton);
    if (pushButton)
        pushButton->setDefault(true);

    // If url is overriden by theme, it's already set and
    // we just check the server type and switch to second page
    // immediately.
    if (Theme::instance()->overrideServerUrl().isEmpty()) {
        _ui.leUrl->setFocus();
    } else {
        setCommitPage(true);
        validatePage();
        setVisible(false);
        // because the wizard will call show on us right after this call, we need to hide in the
        // next event loop iteration.
        QTimer::singleShot(0, this, SLOT(hide()));
    }
}

bool OwncloudSetupPage::urlHasChanged()
{
    bool change = false;
    const QChar slash('/');

    QUrl currentUrl( url() );
    QUrl initialUrl( _oCUrl );

    QString currentPath = currentUrl.path();
    QString initialPath = initialUrl.path();

    // add a trailing slash.
    if( ! currentPath.endsWith( slash )) currentPath += slash;
    if( ! initialPath.endsWith( slash )) initialPath += slash;

    if( currentUrl.host() != initialUrl.host() ||
        currentUrl.port() != initialUrl.port() ||
            currentPath != initialPath ) {
        change = true;
    }

    return change;
}

int OwncloudSetupPage::nextId() const
{
    if (_authType == WizardCommon::HttpCreds) {
        return WizardCommon::Page_HttpCreds;
    } else {
        return WizardCommon::Page_ShibbolethCreds;
    }
}

QString OwncloudSetupPage::url() const
{
    QString url = _ui.leUrl->text().simplified();
    return url;
}

bool OwncloudSetupPage::validatePage()
{
    if( ! _authTypeKnown) {
        setErrorString(QString::null);
        _checking = true;
        startSpinner ();
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

void OwncloudSetupPage::setAuthType (WizardCommon::AuthType type)
{
  _authTypeKnown = true;
  _authType = type;
  stopSpinner();
}

void OwncloudSetupPage::setErrorString( const QString& err )
{
    if( err.isEmpty()) {
        _ui.errorLabel->setVisible(false);
    } else {
        if (_ui.leUrl->text().startsWith("https://")) {
            QString msg = tr("<p>Could not connect securely:</p><p>%1</p><p>Do you want to connect unencrypted instead (not recommended)?</p>").arg(err);
            QString title = tr("Connection failed");
            if (QMessageBox::question(this, title, msg, QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes) {
                QUrl url(_ui.leUrl->text());
                url.setScheme("http");
                _ui.leUrl->setText(url.toString());
                // skip ahead to next page, since the user would expect us to retry automatically
                wizard()->next();
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

void OwncloudSetupPage::setConfigExists(  bool config )
{
    if (config == true) {
        setSubTitle(WizardCommon::subTitleTemplate().arg(tr("Update %1 server")
                                                         .arg(Theme::instance()->appNameGUI())));
    }
}

} // namespace OCC
