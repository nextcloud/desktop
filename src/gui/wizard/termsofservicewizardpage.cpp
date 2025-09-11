/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
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
    const auto ocWizard = qobject_cast<OwncloudWizard *>(wizard());
    Q_ASSERT(ocWizard);

    if (ocWizard->useVirtualFileSyncByDefault()) {
        return -1;
    }

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

