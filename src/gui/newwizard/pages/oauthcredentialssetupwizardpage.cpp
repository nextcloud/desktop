/*
 * Copyright (C) Fabian Müller <fmueller@owncloud.com>
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

#include "oauthcredentialssetupwizardpage.h"

#include "gui/creds/qmlcredentials.h"
#include "gui/qmlutils.h"

#include <QHBoxLayout>

namespace OCC::Wizard {

OAuthCredentialsSetupWizardPage::OAuthCredentialsSetupWizardPage(OAuth *oauth, const QUrl &serverUrl, QWidget *parent)
    : AbstractSetupWizardPage(parent)
{
    auto *layout = new QHBoxLayout(this);
    auto *widget = new QmlUtils::OCQuickWidget;
    layout->addWidget(widget);
    setFocusProxy(widget);

    auto *oauthCredentials = new QmlOAuthCredentials(oauth, serverUrl, {});
    oauthCredentials->setIsRefresh(false);
    widget->setOCContext(
        QUrl(QStringLiteral("qrc:/qt/qml/org/ownCloud/gui/qml/credentials/OAuthCredentials.qml")), this, oauthCredentials, QJSEngine::JavaScriptOwnership);
}

bool OAuthCredentialsSetupWizardPage::validateInput()
{
    // in this special case, the input may never be validated, i.e., the next button also never needs to be enabled
    // an external system set up by the controller will move to the next page in the background
    return false;
}

} // namespace OCC::Wizard
