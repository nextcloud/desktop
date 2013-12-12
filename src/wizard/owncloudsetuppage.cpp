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
#include <QPushButton>

#include "QProgressIndicator.h"

#include "wizard/owncloudwizardcommon.h"
#include "wizard/owncloudsetuppage.h"
#include "mirall/theme.h"

namespace Mirall
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
#if 0
    bool visible = false;

    if (url.startsWith(QLatin1String("https://"))) {
        _ui.urlLabel->setPixmap( QPixmap(":/mirall/resources/security-high.png"));
        _ui.urlLabel->setToolTip(tr("This url is secure. You can use it."));
        visible = true;
    }
    if (url.startsWith(QLatin1String("http://"))) {
        _ui.urlLabel->setPixmap( QPixmap(":/mirall/resources/security-low.png"));
        _ui.urlLabel->setToolTip(tr("This url is NOT secure. You should not use it."));
        visible = true;
    }
#endif
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
        setEnabled(false);
        validatePage();
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
    _configExists = config;

    if (config == true) {
        setSubTitle(WizardCommon::subTitleTemplate().arg(tr("Update %1 server")
                                                         .arg(Theme::instance()->appNameGUI())));
    }
}

} // ns Mirall
