/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
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
    enum class FileLocation {
        Default = 0,
        NewLocalFile,
    };
    enum class InvalidMode {
        SystemInvalid,
        ServerInvalid
    };

    explicit InvalidFilenameDialog(AccountPtr account,
                                   Folder *folder,
                                   QString filePath,
                                   FileLocation fileLocation = FileLocation::Default,
                                   InvalidMode invalidMode = InvalidMode::SystemInvalid,
                                   QWidget *parent = nullptr);

    ~InvalidFilenameDialog() override;

    void accept() override;

signals:
    void acceptedInvalidName(const QString &filePath);

private:
    std::unique_ptr<Ui::InvalidFilenameDialog> _ui;

    AccountPtr _account;
    Folder *_folder;
    QString _filePath;
    QString _relativeFilePath;
    QString _originalFileName;
    QString _newFilename;
    FileLocation _fileLocation = FileLocation::Default;

    void onFilenameLineEditTextChanged(const QString &text);
    void onMoveJobFinished();
    void onRemoteDestinationFileAlreadyExists(const QVariantMap &values);
    void onRemoteDestinationFileDoesNotExist(QNetworkReply *reply);
    void onRemoteSourceFileAlreadyExists(const QVariantMap &values);
    void onRemoteSourceFileDoesNotExist(QNetworkReply *reply);
    void checkIfAllowedToRename();
    void onCheckIfAllowedToRenameComplete(const QVariantMap &values, QNetworkReply *reply = nullptr);
    bool processLeadingOrTrailingSpacesError(const QString &fileName);
    void onPropfindPermissionSuccess(const QVariantMap &values);
    void onPropfindPermissionError(QNetworkReply *reply = nullptr);
    void allowRenaming();
private slots:
    void useInvalidName();
};
}
