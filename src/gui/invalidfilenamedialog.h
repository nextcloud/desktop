/*
 * Copyright (C) by Felix Weilbach <felix.weilbach@nextcloud.com>
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

#pragma once

#include <accountfwd.h>
#include <account.h>

#include <memory>

#include <QDialog>

namespace OCC {

class Folder;

namespace Ui {
    class InvalidFilenameDialog;
}


class InvalidFilenameDialog : public QDialog
{
    Q_OBJECT

public:
    explicit InvalidFilenameDialog(AccountPtr account, Folder *folder, QString filePath, QWidget *parent = nullptr);

    ~InvalidFilenameDialog() override;

    void accept() override;

private:
    std::unique_ptr<Ui::InvalidFilenameDialog> _ui;

    AccountPtr _account;
    Folder *_folder;
    QString _filePath;
    QString _relativeFilePath;
    QString _originalFileName;
    QString _newFilename;

    void onFilenameLineEditTextChanged(const QString &text);
    void onMoveJobFinished();
    void onRemoteFileAlreadyExists(const QVariantMap &values);
    void onRemoteFileDoesNotExist(QNetworkReply *reply);
    void checkIfAllowedToRename();
    void onPropfindPermissionSuccess(const QVariantMap &values);
};
}
