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

    QString HttpCredentialsGui::queryPassword(bool *ok, const QString& hint)
{
    if (!ok) {
        return QString();
    }

    QString msg = tr("Please enter %1 password:\n"
                     "\n"
                     "User: %2\n"
                     "Account: %3\n")
                  .arg(Theme::instance()->appNameGUI(), _user, _account->displayName());
    if (!hint.isEmpty()) {
        msg += QLatin1String("\n") + hint + QLatin1String("\n");
    }

    return QInputDialog::getText(0, tr("Enter Password"), msg,
                                 QLineEdit::Password, _previousPassword,
                                 ok);
}

} // namespace OCC
