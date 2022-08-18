/*
 * Copyright (C) by Klaas Freitag <freitag@kde.org>
 * Copyright (C) by Olivier Goffart <ogoffart@woboq.com>
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

#include "creds/httpcredentialsgui.h"
#include "account.h"
#include "application.h"
#include "basicloginwidget.h"
#include "common/asserts.h"
#include "loginrequireddialog.h"
#include "networkjobs.h"
#include "settingsdialog.h"
#include "theme.h"

#include <QBuffer>
#include <QDesktopServices>
#include <QMessageBox>
#include <QNetworkReply>
#include <QTimer>


namespace OCC {

Q_LOGGING_CATEGORY(lcHttpCredentialsGui, "sync.credentials.http.gui", QtInfoMsg)

void HttpCredentialsGui::openBrowser()
{
    OC_ASSERT(isUsingOAuth());
    if (isUsingOAuth()) {
        if (_asyncAuth) {
            _asyncAuth->openBrowser();
        } else {
            qCWarning(lcHttpCredentialsGui) << "There is no authentication job running, did the previous attempt fail?";
            askFromUserAsync();
        }
    }
}

void HttpCredentialsGui::askFromUser()
{
    // This function can be called from AccountState::slotInvalidCredentials,
    // which (indirectly, through HttpCredentials::invalidateToken) schedules
    // a cache wipe of the qnam. We can only execute a network job again once
    // the cache has been cleared, otherwise we'd interfere with the job.
    QTimer::singleShot(0, this, &HttpCredentialsGui::askFromUserAsync);
}

void HttpCredentialsGui::askFromUserAsync()
{
    const auto updateOAuth = [this] {
        _asyncAuth.reset(new AccountBasedOAuth(_account->sharedFromThis(), this));
        connect(_asyncAuth.data(), &OAuth::result,
            this, &HttpCredentialsGui::asyncAuthResult);
        connect(_asyncAuth.data(), &OAuth::destroyed,
            this, &HttpCredentialsGui::authorisationLinkChanged);
        _asyncAuth->startAuthentication();
        emit authorisationLinkChanged();
    };
    if (isUsingOAuth()) {
        updateOAuth();
    } else {
        // First, we will check what kind of auth we need.
        auto job = new DetermineAuthTypeJob(_account->sharedFromThis(), this);
        QObject::connect(job, &DetermineAuthTypeJob::authType, this, [updateOAuth, this](DetermineAuthTypeJob::AuthType type) {
            _authType = type;
            if (type == DetermineAuthTypeJob::AuthType::OAuth) {
                updateOAuth();
            } else if (type == DetermineAuthTypeJob::AuthType::Basic) {
                showDialog();
            } else {
                qCWarning(lcHttpCredentialsGui) << "Bad http auth type:" << type;
                emit asked();
            }
        });
        job->start();
    }
}

void HttpCredentialsGui::asyncAuthResult(OAuth::Result r, const QString &user,
    const QString &token, const QString &refreshToken, const QString &)
{
    _asyncAuth.reset();
    switch (r) {
    case OAuth::NotSupported:
        showDialog();
        return;
    case OAuth::Error:
        emit asked();
        return;
    case OAuth::LoggedIn:
        break;
    }

    OC_ASSERT(_user == user); // ensured by _asyncAuth

    _password = token;
    _refreshToken = refreshToken;
    _ready = true;
    persist();
    emit asked();
}

void HttpCredentialsGui::showDialog()
{
    auto *contentWidget = new BasicLoginWidget();
    contentWidget->setServerUrl(_account->url());
    contentWidget->forceUsername(_account->davUser());

    auto *dialog = new LoginRequiredDialog(contentWidget, ocApp()->gui()->settingsDialog());
    dialog->setAttribute(Qt::WA_DeleteOnClose, true);
    dialog->setTopLabelText(tr("Please enter your password to log in to your account."));

    // in this case, we want to use the login button
    dialog->addLogInButton();

    connect(dialog, &LoginRequiredDialog::finished, ocApp()->gui()->settingsDialog(), [this, contentWidget](const int result) {
        if (result == QDialog::Accepted) {
            Q_ASSERT(contentWidget->username() == _account->davUser());
            _password = contentWidget->password();
            _refreshToken.clear();
            _ready = true;
            persist();
        } else {
            Q_EMIT requestLogout();
        }
        emit asked();
    });

    dialog->open();
    ownCloudGui::raiseDialog(dialog);

    QTimer::singleShot(0, [contentWidget]() {
        contentWidget->setFocus(Qt::OtherFocusReason);
    });
}

QUrl HttpCredentialsGui::authorisationLink() const
{
    OC_ASSERT(isUsingOAuth());
    if (isUsingOAuth()) {
        if (_asyncAuth) {
            return _asyncAuth->authorisationLink();
        } else {
            qCWarning(lcHttpCredentialsGui) << "There is no authentication job running, did the previous attempt fail?";
            return {};
        }
    }
    return {};
}

} // namespace OCC
