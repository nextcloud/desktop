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

#include <QInputDialog>
#include <QLabel>
#include "creds/httpcredentialsgui.h"
#include "theme.h"
#include "account.h"

using namespace QKeychain;

namespace OCC
{

void HttpCredentialsGui::askFromUser()
{
    // The rest of the code assumes that this will be done asynchronously
    QMetaObject::invokeMethod(this, "askFromUserAsync", Qt::QueuedConnection);
}

void HttpCredentialsGui::askFromUserAsync()
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
        msg += QLatin1String("<br>") + tr("Reading from keychain failed with error: '%1'").arg(
                    Utility::escape(_fetchErrorString)) + QLatin1String("<br>");
    }

    QInputDialog dialog;
    dialog.setWindowTitle(tr("Enter Password"));
    dialog.setLabelText(msg);
    dialog.setTextValue(_previousPassword);
    dialog.setTextEchoMode(QLineEdit::Password);
    if (QLabel *dialogLabel = dialog.findChild<QLabel *>()) {
        dialogLabel->setOpenExternalLinks(true);
        dialogLabel->setTextFormat(Qt::RichText);
    }

    bool ok = dialog.exec();
    if (ok) {
        _password = dialog.textValue();
        _ready = true;
        persist();
    }
    emit asked();
}

QString HttpCredentialsGui::requestAppPasswordText(const Account* account)
{
    if (account->serverVersionInt() < Account::makeServerVersion(9, 1, 0)) {
        // Older server than 9.1 does not have the feature to request App Password
        return QString();
    }

    QString path;
    if (account->serverVersionInt() < Account::makeServerVersion(10, 0, 0)) {
        path = QLatin1String("/index.php/settings/personal?section=apppasswords");
    } else {
        path = QLatin1String("/index.php/settings/personal?sectionid=security#apppasswords");
    }
    return tr("<a href=\"%1\">Click here</a> to request an app password from the web interface.")
        .arg(account->url().toString() + path);
}


} // namespace OCC
