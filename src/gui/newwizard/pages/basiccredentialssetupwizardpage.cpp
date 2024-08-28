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

#include "basiccredentialssetupwizardpage.h"

#include "gui/creds/qmlcredentials.h"
#include "gui/qmlutils.h"

#include <QHBoxLayout>


namespace OCC::Wizard {

BasicCredentialsSetupWizardPage::BasicCredentialsSetupWizardPage(const QUrl &serverUrl, QWidget *parent)
    : AbstractSetupWizardPage(parent)
{
    auto *layout = new QHBoxLayout(this);
    auto *widget = new QmlUtils::OCQuickWidget;
    layout->addWidget(widget);
    setFocusProxy(widget);

    auto *basicCredentials = new QmlBasicCredentials(serverUrl, _userName, widget);
    basicCredentials->setIsRefresh(false);
    if (!_userName.isEmpty()) {
        basicCredentials->setReadOnlyName(_userName);
    }
    widget->setOCContext(
        QUrl(QStringLiteral("qrc:/qt/qml/org/ownCloud/gui/qml/credentials/BasicAuthCredentials.qml")), this, basicCredentials, QJSEngine::JavaScriptOwnership);
    connect(basicCredentials, &QmlBasicCredentials::readyChanged, this, [basicCredentials, this] {
        _userName = basicCredentials->userName();
        _password = basicCredentials->password();
        Q_EMIT contentChanged();
    });
}

BasicCredentialsSetupWizardPage *BasicCredentialsSetupWizardPage::createForWebFinger(const QUrl &serverUrl, const QString &username)
{
    auto page = new BasicCredentialsSetupWizardPage(serverUrl);
    page->_userName = username;
    return page;
}

QString BasicCredentialsSetupWizardPage::username() const
{
    return _userName;
}

QString BasicCredentialsSetupWizardPage::password() const
{
    return _password;
}

bool BasicCredentialsSetupWizardPage::validateInput()
{
    return !(username().isEmpty() || password().isEmpty());
}
}
