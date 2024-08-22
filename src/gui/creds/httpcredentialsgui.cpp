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
#include "accountmodalwidget.h"
#include "application.h"
#include "common/asserts.h"
#include "gui/accountsettings.h"
#include "gui/loginrequireddialog/basicloginwidget.h"
#include "gui/loginrequireddialog/loginrequireddialog.h"
#include "gui/loginrequireddialog/oauthloginwidget.h"
#include "networkjobs.h"
#include "settingsdialog.h"

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
    // This function can be called from AccountState::slotInvalidCredentials,
    // which (indirectly, through HttpCredentials::invalidateToken) schedules
    // a cache wipe of the qnam. We can only execute a network job again once
    // the cache has been cleared, otherwise we'd interfere with the job.
    QTimer::singleShot(0, this, [this] {
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
                    Q_EMIT fetched();
                }
            });
            job->start();
        }
    });
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
        Q_EMIT oAuthLoginAccepted();
        break;
    }

    _password = token;
    _refreshToken = refreshToken;
    _ready = true;
    persist();
    Q_EMIT fetched();
}

void HttpCredentialsGui::showDialog()
{
    if (_loginRequiredDialog != nullptr) {
        return;
    }

    auto *dialog = new LoginRequiredDialog(LoginRequiredDialog::Mode::Basic, ocApp()->gui()->settingsDialog());

    // make sure it's cleaned up since it's not owned by the account settings (also prevents memory leaks)
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    dialog->setTopLabelText(tr("Please enter your password to log in to the account %1.").arg(_account->displayNameWithHost()));

    auto *contentWidget = qobject_cast<BasicLoginWidget *>(dialog->contentWidget());
    contentWidget->forceUsername(user());

    auto *modalWidget = new AccountModalWidget(tr("Login required"), dialog, ocApp()->gui()->settingsDialog());
    modalWidget->addButton(tr("Log out"), QDialogButtonBox::RejectRole);
    modalWidget->addButton(tr("Log in"), QDialogButtonBox::AcceptRole); // in this case, we want to use the login button
    connect(this, &HttpCredentialsGui::oAuthLoginAccepted, modalWidget, &AccountModalWidget::accept);
    connect(modalWidget, &AccountModalWidget::finished, ocApp()->gui()->settingsDialog(), [this, contentWidget](AccountModalWidget::Result result) {
        if (result == AccountModalWidget::Result::Accepted) {
            _password = contentWidget->password();
            _refreshToken.clear();
            _ready = true;
            persist();
        } else {
            Q_EMIT requestLogout();
        }
        Q_EMIT fetched();
    });

    ocApp()->gui()->settingsDialog()->accountSettings(_account)->addModalWidget(modalWidget);
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
            tr("The account %1 is currently logged out.\n\nPlease authenticate using your browser.").arg(_account->displayNameWithHost()));

        auto *contentWidget = qobject_cast<OAuthLoginWidget *>(_loginRequiredDialog->contentWidget());
        connect(contentWidget, &OAuthLoginWidget::openBrowserButtonClicked, this, &HttpCredentialsGui::openBrowser);

        connect(contentWidget, &OAuthLoginWidget::retryButtonClicked, _loginRequiredDialog, [contentWidget, this]() {
            restartOAuth();
            contentWidget->hideRetryFrame();
        });

        connect(this, &HttpCredentialsGui::oAuthErrorOccurred, _loginRequiredDialog, [contentWidget, this]() {
            Q_ASSERT(!ready());
            ownCloudGui::raise();
            contentWidget->showRetryFrame();
        });

        auto *modalWidget = new AccountModalWidget(tr("Login required"), _loginRequiredDialog, ocApp()->gui()->settingsDialog());
        modalWidget->addButton(tr("Log out"), QDialogButtonBox::RejectRole);
        connect(this, &HttpCredentialsGui::oAuthLoginAccepted, modalWidget, &AccountModalWidget::accept);
        connect(modalWidget, &AccountModalWidget::rejected, this, &HttpCredentials::requestLogout);

        ocApp()->gui()->settingsDialog()->accountSettings(_account)->addModalWidget(modalWidget);
    }

    _asyncAuth.reset(new AccountBasedOAuth(_account->sharedFromThis(), this));
    connect(_asyncAuth.data(), &OAuth::result, this, &HttpCredentialsGui::asyncAuthResult);
    connect(_asyncAuth.data(), &OAuth::authorisationLinkChanged, this,
        [this] { qobject_cast<OAuthLoginWidget *>(_loginRequiredDialog->contentWidget())->setUrl(authorisationLink()); });
    _asyncAuth->startAuthentication();
}

} // namespace OCC
