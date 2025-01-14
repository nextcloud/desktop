/*
 * Copyright (C) 2021 by Felix Weilbach <felix.weilbach@nextcloud.com>
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

#include "welcomepage.h"
#include "buttonstyle.h"
#include "guiutility.h"
#include "theme.h"
#include "ui_welcomepage.h"
#include "wizard/owncloudwizard.h"
#include "wizard/slideshow.h"

namespace OCC {

WelcomePage::WelcomePage(OwncloudWizard *ocWizard)
    : QWizardPage()
    , _ui(new Ui::WelcomePage)
    , _ocWizard(ocWizard)
{
    setupUi();
}

WelcomePage::~WelcomePage() = default;

void WelcomePage::setupUi()
{
    _ui->setupUi(this);
    setupSlideShow();
    setupLoginButton();
    setupCreateAccountButton();
    setupHostYourOwnServerLabel();
}

void WelcomePage::initializePage()
{
    customizeStyle();
}

void WelcomePage::setLoginButtonDefault()
{
#ifdef Q_OS_WIN
    _ui->loginButton->setDefault(true);
#endif
    _ui->loginButton->setFocus();
}

void WelcomePage::styleSlideShow()
{
    const auto ionosLogoFileName = Theme::hidpiFileName(":/client/theme/colored/IONOS_logo_w_suffix_frontend.png");

    _ui->slideShow->addSlide(ionosLogoFileName, tr("Keep your data secure and under your control")); 
}

void WelcomePage::setupSlideShow()
{
    _ui->slideShowNextButton->hide();
    _ui->slideShowPreviousButton->hide();
    connect(_ui->slideShow, &SlideShow::clicked, _ui->slideShow, &SlideShow::stopShow);
    connect(_ui->slideShowNextButton, &QPushButton::clicked, _ui->slideShow, &SlideShow::nextSlide);
    connect(_ui->slideShowPreviousButton, &QPushButton::clicked, _ui->slideShow, &SlideShow::prevSlide);
}

void WelcomePage::setupLoginButton()
{
    _ui->loginButton->setProperty("buttonStyle", QVariant::fromValue(OCC::ButtonStyleName::Primary));
    connect(_ui->loginButton, &QPushButton::clicked, this, [this](bool /*checked*/) {
        _nextPage = WizardCommon::Page_ServerSetup;
        _ocWizard->next(); 
    });
}

void WelcomePage::setupCreateAccountButton()
{
    _ui->createAccountButton->hide();
#ifdef WITH_WEBENGINE
    connect(_ui->createAccountButton, &QPushButton::clicked, this, [this](bool /*checked*/) {
        _ocWizard->setRegistration(true);
        _nextPage = WizardCommon::Page_WebView;
        _ocWizard->setAuthType(OCC::DetermineAuthTypeJob::WebViewFlow);
    });
#else // WITH_WEBENGINE
    connect(_ui->createAccountButton, &QPushButton::clicked, this, [this](bool /*checked*/) {
        _ocWizard->setRegistration(true);
        Utility::openBrowser(QStringLiteral("https://nextcloud.com/register"));
    });
#endif // WITH_WEBENGINE
}

void WelcomePage::setupHostYourOwnServerLabel()
{
    _ui->hostYourOwnServerLabel->hide();
    _ui->hostYourOwnServerLabel->setText(tr("Host your own server"));
    _ui->hostYourOwnServerLabel->setAlignment(Qt::AlignCenter);  
    _ui->hostYourOwnServerLabel->setUrl(QUrl("https://docs.nextcloud.com/server/latest/admin_manual/installation/#installation"));
}

int WelcomePage::nextId() const
{
    return _nextPage;
}

void WelcomePage::customizeStyle()
{
    _ocWizard->setFixedSize(626, 460);
    _ui->mainHbox->setContentsMargins(0, 0, 0, 0);   
    styleSlideShow();
}
}
