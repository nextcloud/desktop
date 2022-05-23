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

#include "application.h"
#include "account.h"
#include "common/asserts.h"
#include "creds/httpcredentialsgui.h"
#include "networkjobs.h"
#include "settingsdialog.h"
#include "theme.h"

#include <QBuffer>
#include <QDesktopServices>
#include <QInputDialog>
#include <QLabel>
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
    QString msg = tr("Please enter %1 password:<br>"
                     "<br>"
                     "User: %2<br>"
                     "Account: %3<br>")
                      .arg(Utility::escape(Theme::instance()->appNameGUI()),
                          Utility::escape(_user),
                          Utility::escape(_account->displayName()));

    QString reqTxt = requestAppPasswordText(_account);
    if (!reqTxt.isEmpty()) {
        msg += QLatin1String("<br>") + reqTxt + QLatin1String("<br>");
    }
    if (!_fetchErrorString.isEmpty()) {
        msg += QLatin1String("<br>")
            + tr("Reading from keychain failed with error: '%1'")
                  .arg(Utility::escape(_fetchErrorString))
            + QLatin1String("<br>");
    }

    QInputDialog *dialog = new QInputDialog(ocApp()->gui()->settingsDialog());
    dialog->setAttribute(Qt::WA_DeleteOnClose, true);
    dialog->setWindowTitle(tr("Enter Password"));
    dialog->setLabelText(msg);
    dialog->setTextValue(_previousPassword);
    dialog->setTextEchoMode(QLineEdit::Password);
    if (QLabel *dialogLabel = dialog->findChild<QLabel *>()) {
        dialogLabel->setOpenExternalLinks(true);
        dialogLabel->setTextFormat(Qt::RichText);
    }

    connect(dialog, &QDialog::finished, this, [this, dialog](int result) {
        if (result == QDialog::Accepted) {
            _password = dialog->textValue();
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
}

QString HttpCredentialsGui::requestAppPasswordText(const Account *account)
{
    auto baseUrl = account->url().toString();
    if (baseUrl.endsWith('/'))
        baseUrl.chop(1);
    return tr("<a href=\"%1\">Click here</a> to request an app password from the web interface.")
        .arg(Utility::concatUrlPath(baseUrl, QStringLiteral("/index.php/settings/personal?sectionid=security#apppasswords")).toString());
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
