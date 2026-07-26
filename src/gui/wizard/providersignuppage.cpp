/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "providersignuppage.h"

#include "wizard/flow2authwidget.h"

#include <QUrl>
#include <QVBoxLayout>

namespace OCC {

ProviderSignupPage::ProviderSignupPage(QWidget *parent)
    : QWizardPage(parent)
    , _layout(new QVBoxLayout(this))
    , _flow2AuthWidget(new Flow2AuthWidget)
{
    _layout->addWidget(_flow2AuthWidget);
}

void ProviderSignupPage::initializePage()
{
    _flow2AuthWidget->startProviderSignup(QUrl(QStringLiteral("https://nextcloud.com/sign-up/?flow=V3")));
    _flow2AuthWidget->slotStyleChanged();
}

void ProviderSignupPage::cleanupPage()
{
    _flow2AuthWidget->resetAuth();
}

bool ProviderSignupPage::isComplete() const
{
    return false;
}

void ProviderSignupPage::slotStyleChanged()
{
    _flow2AuthWidget->slotStyleChanged();
}

} // namespace OCC
