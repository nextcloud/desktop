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
