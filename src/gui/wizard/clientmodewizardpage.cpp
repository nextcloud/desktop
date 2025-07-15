/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "clientmodewizardpage.h"

#include "wizard/owncloudwizard.h"
#include "wizard/owncloudwizardcommon.h"
#include "theme.h"

#include <QVBoxLayout>
#include <QWidget>
#include <QWizardPage>
#include <QQuickView>
#include <QQmlApplicationEngine>

namespace OCC {

ClientModeWizardPage::ClientModeWizardPage()
    : QWizardPage()
{
    qmlRegisterSingletonInstance("com.nextcloud.desktopclient", 1, 0, "ClientTheme", Theme::instance());
    _layout = new QVBoxLayout(this);
    _quickView = new QQuickView;
    _quickView->setSource(QUrl(QStringLiteral("qrc:/qml/src/gui/wizard/ClientModeWizardPage.qml")));
    _windowContainer = QWidget::createWindowContainer(_quickView, this);
    _windowContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    _layout->addWidget(_windowContainer);
    setLayout(_layout);
}

void ClientModeWizardPage::initializePage()
{
    _ocWizard = qobject_cast<OwncloudWizard *>(wizard());
    Q_ASSERT(_ocWizard);

    show();
}

void ClientModeWizardPage::cleanupPage()
{
    // Cleanup code can go here if needed
    // For example, disconnect signals or delete temporary objects
}

int ClientModeWizardPage::nextId() const
{
    return WizardCommon::Page_AdvancedSetup;
}


bool ClientModeWizardPage::isComplete() const
{
    // Logic to determine if the page is complete
    return true; // Adjust based on actual completion criteria
}

} // namespace OCC
