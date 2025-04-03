/*
 * Copyright (C) by Matthieu Gallien <matthieu.gallien@nextcloud.com>
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

#include "termsofservicewizardpage.h"

#include "account.h"
#include "owncloudsetupwizard.h"
#include "wizard/owncloudwizard.h"
#include "wizard/owncloudwizardcommon.h"
#include "wizard/termsofservicecheckwidget.h"
#include "connectionvalidator.h"

#include <QVBoxLayout>
#include <QDesktopServices>

namespace OCC {

OCC::TermsOfServiceWizardPage::TermsOfServiceWizardPage()
    : QWizardPage()
{
    _layout = new QVBoxLayout(this);

    _termsOfServiceCheckWidget = new TermsOfServiceCheckWidget;
    _layout->addWidget(_termsOfServiceCheckWidget);

    connect(this, &TermsOfServiceWizardPage::styleChanged, _termsOfServiceCheckWidget, &TermsOfServiceCheckWidget::slotStyleChanged);
    connect(_termsOfServiceCheckWidget, &TermsOfServiceCheckWidget::pollNow, this, &TermsOfServiceWizardPage::slotPollNow);
}

void OCC::TermsOfServiceWizardPage::initializePage()
{
    _ocWizard = qobject_cast<OwncloudWizard *>(wizard());
    Q_ASSERT(_ocWizard);

    _termsOfServiceChecker = new TermsOfServiceChecker{_ocWizard->account(), this};
    connect(_termsOfServiceChecker, &TermsOfServiceChecker::done, this, &TermsOfServiceWizardPage::termsOfServiceChecked);

    _termsOfServiceCheckWidget->setUrl(_ocWizard->account()->url());
    _termsOfServiceCheckWidget->slotStyleChanged();
    _termsOfServiceCheckWidget->start();

    connect(_ocWizard, &OwncloudWizard::onActivate, this, &TermsOfServiceWizardPage::slotPollNow);
}

void OCC::TermsOfServiceWizardPage::cleanupPage()
{
    disconnect(_ocWizard, &OwncloudWizard::onActivate, this, &TermsOfServiceWizardPage::slotPollNow);

    _termsOfServiceChecker->deleteLater();
    _termsOfServiceChecker = nullptr;
}

int OCC::TermsOfServiceWizardPage::nextId() const
{
    return WizardCommon::Page_AdvancedSetup;
}

bool OCC::TermsOfServiceWizardPage::isComplete() const
{
    return false;
}

void TermsOfServiceWizardPage::slotPollNow()
{
    if (!_termsOfServiceChecker) {
        return;
    }

    _termsOfServiceChecker->start();
}

void TermsOfServiceWizardPage::termsOfServiceChecked()
{
    if (_termsOfServiceChecker && _termsOfServiceChecker->needToSign()) {
        _termsOfServiceCheckWidget->termsNotAcceptedYet();
        return;
    }
    _ocWizard->successfulStep();
}

}

