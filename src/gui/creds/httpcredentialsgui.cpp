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
    QString msg = tr("Please enter %1 password:\n"
                     "\n"
                     "User: %2\n"
                     "Account: %3\n")
                  .arg(Theme::instance()->appNameGUI(), _user, _account->displayName());
    if (!_fetchErrorString.isEmpty()) {
        msg += QLatin1String("\n") + tr("Reading from keychain failed with error: '%1'").arg(
                    _fetchErrorString) + QLatin1String("\n");
    }

    bool ok = false;
    QString pwd = QInputDialog::getText(0, tr("Enter Password"), msg,
                                 QLineEdit::Password, _previousPassword,
                                 &ok);
    if (ok) {
        _password = pwd;
        _ready = true;
        persist();
    }
    emit asked();
}

} // namespace OCC
