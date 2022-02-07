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

    auto matchUrl = [](QUrl url1, QUrl url2) {
        // ensure https://demo.owncloud.org/ matches https://demo.owncloud.org
        // the empty path was the legacy formating before 2.9
        if (url1.path().isEmpty()) {
            url1.setPath(QStringLiteral("/"));
        }
        if (url2.path().isEmpty()) {
            url2.setPath(QStringLiteral("/"));
        }

        return url1.matches(url2, QUrl::StripTrailingSlash | QUrl::NormalizePathSegments);
    };

    if (matchUrl(_oldUrl, _newUrl)) {
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

}
