/*
* Copyright (C) by Fabian Müller <fmueller@owncloud.com>
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

#include "updateurldialog.h"

#include <QMessageBox>
#include <QPushButton>
#include <QTimer>

namespace OCC {

UpdateUrlDialog::UpdateUrlDialog(const QString &title, const QString &content, const QUrl &oldUrl, const QUrl &newUrl, QWidget *parent)
    : QMessageBox(QMessageBox::Warning, title, content, QMessageBox::NoButton, parent)
    , _oldUrl(oldUrl)
    , _newUrl(newUrl)
{
    // this special dialog deletes itself after use
    setAttribute(Qt::WA_DeleteOnClose);

    if (Utility::urlEqual(_oldUrl, _newUrl)) {
        // need to show the dialog before accepting the change
        // hence using a timer to run the code on the main loop
        QTimer::singleShot(0, [this]() {
            accept();
        });
        return;
    }

    addButton(tr("Change url permanently to %1").arg(_newUrl.toString()), QMessageBox::AcceptRole);
    addButton(tr("Reject"), QMessageBox::RejectRole);
}

UpdateUrlDialog *UpdateUrlDialog::fromAccount(AccountPtr account, const QUrl &newUrl, QWidget *parent)
{
    return new UpdateUrlDialog(
        tr("Url update requested for %1").arg(account->displayName()),
        tr("The url for %1 changed from %2 to %3, do you want to accept the changed url?").arg(account->displayName(), account->url().toString(), newUrl.toString()),
        account->url(),
        newUrl,
        parent);
}
}
