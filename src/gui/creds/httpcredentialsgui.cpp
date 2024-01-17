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
#include "common/asserts.h"
#include "gui/loginrequireddialog/basicloginwidget.h"
#include "gui/loginrequireddialog/loginrequireddialog.h"
#include "gui/loginrequireddialog/oauthloginwidget.h"
#include "networkjobs.h"
#include "settingsdialog.h"
#include "theme.h"

#include <QClipboard>
#include <QDesktopServices>
#include <QGuiApplication>
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
            askFromUser();
        }
    }
}

void HttpCredentialsGui::askFromUser()
{
    if (isUsingOAuth()) {
        restartOAuth();
    } else {
        // First, we will check what kind of auth we need.
        auto job = new DetermineAuthTypeJob(_account->sharedFromThis(), this);
        QObject::connect(job, &DetermineAuthTypeJob::authType, this, [this](DetermineAuthTypeJob::AuthType type) {
            _authType = type;
            if (type == DetermineAuthTypeJob::AuthType::OAuth) {
                restartOAuth();
            } else if (type == DetermineAuthTypeJob::AuthType::Basic) {
                showDialog();
            } else {
                qCWarning(lcHttpCredentialsGui) << "Bad http auth type:" << type;
                emit fetched();
            }
        });
        job->start();
    }
}

void HttpCredentialsGui::asyncAuthResult(OAuth::Result r, const QString &token, const QString &refreshToken)
{
    _asyncAuth.reset();
    switch (r) {
    case OAuth::NotSupported:
        if (_loginRequiredDialog) {
            _loginRequiredDialog->deleteLater();
            _loginRequiredDialog.clear();
        }
        // show basic auth dialog for historic reasons
        showDialog();
        return;
    case OAuth::ErrorInsecureUrl:
        // should not happen after the initial setup
        Q_ASSERT(false);
        [[fallthrough]];
    case OAuth::Error:
        Q_EMIT oAuthErrorOccurred();
        return;
    case OAuth::LoggedIn:
        if (_loginRequiredDialog) {
            _loginRequiredDialog->accept();
        }
        break;
    }

    _password = token;
    _refreshToken = refreshToken;
    _ready = true;
    persist();
    emit fetched();
}

void HttpCredentialsGui::showDialog()
{
    if (_loginRequiredDialog != nullptr) {
        return;
    }

    auto *dialog = new LoginRequiredDialog(LoginRequiredDialog::Mode::Basic, ocApp()->gui()->settingsDialog());

    // make sure it's cleaned up since it's not owned by the account settings (also prevents memory leaks)
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    dialog->setTopLabelText(tr("Please enter your password to log in to the account %1.").arg(_account->displayName()));

    auto *contentWidget = qobject_cast<BasicLoginWidget *>(dialog->contentWidget());
    contentWidget->forceUsername(user());

    // in this case, we want to use the login button
    dialog->addLogInButton();

    connect(dialog, &LoginRequiredDialog::finished, ocApp()->gui()->settingsDialog(), [this, contentWidget](const int result) {
        if (result == QDialog::Accepted) {
            _password = contentWidget->password();
            _refreshToken.clear();
            _ready = true;
            persist();
        } else {
            Q_EMIT requestLogout();
        }
        emit fetched();
    });

    dialog->open();
    ownCloudGui::raiseDialog(dialog);

    QTimer::singleShot(0, contentWidget, [contentWidget]() {
        contentWidget->setFocus(Qt::OtherFocusReason);
    });

    _loginRequiredDialog = dialog;
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

void HttpCredentialsGui::restartOAuth()
{
    qCDebug(lcHttpCredentialsGui) << "showing modal dialog asking user to log in again via OAuth2";

    if (!_loginRequiredDialog) {
        _loginRequiredDialog = new LoginRequiredDialog(LoginRequiredDialog::Mode::OAuth, ocApp()->gui()->settingsDialog());

        // make sure it's cleaned up since it's not owned by the account settings (also prevents memory leaks)
        _loginRequiredDialog->setAttribute(Qt::WA_DeleteOnClose);

        _loginRequiredDialog->setTopLabelText(
            tr("The account %1 is currently logged out.\n\nPlease authenticate using your browser.").arg(_account->displayName()));

        auto *contentWidget = qobject_cast<OAuthLoginWidget *>(_loginRequiredDialog->contentWidget());
        connect(contentWidget, &OAuthLoginWidget::copyUrlToClipboardButtonClicked, _loginRequiredDialog, [](const QUrl &url) {
            // TODO: use authorisationLinkAsync
            qApp->clipboard()->setText(url.toString());
        });

        connect(contentWidget, &OAuthLoginWidget::openBrowserButtonClicked, this, &HttpCredentialsGui::openBrowser);
        connect(_loginRequiredDialog, &LoginRequiredDialog::rejected, this, &HttpCredentials::requestLogout);

        connect(contentWidget, &OAuthLoginWidget::retryButtonClicked, _loginRequiredDialog, [contentWidget, this]() {
            restartOAuth();
            contentWidget->hideRetryFrame();
        });

        connect(this, &HttpCredentialsGui::oAuthErrorOccurred, _loginRequiredDialog, [contentWidget, this]() {
            Q_ASSERT(!ready());
            ocApp()->gui()->raiseDialog(_loginRequiredDialog);
            contentWidget->showRetryFrame();
        });

        _loginRequiredDialog->open();
        ocApp()->gui()->raiseDialog(_loginRequiredDialog);
    }

    _asyncAuth.reset(new AccountBasedOAuth(_account->sharedFromThis(), this));
    connect(_asyncAuth.data(), &OAuth::result, this, &HttpCredentialsGui::asyncAuthResult);
    connect(_asyncAuth.data(), &OAuth::authorisationLinkChanged, this,
        [this] { qobject_cast<OAuthLoginWidget *>(_loginRequiredDialog->contentWidget())->setUrl(authorisationLink()); });
    _asyncAuth->startAuthentication();
}

} // namespace OCC
