/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QLoggingCategory>
#include <QtCore>

#include "accountstate.h"

namespace OCC {

Q_DECLARE_LOGGING_CATEGORY(lcFileActions)

class FileActionsModel : public QAbstractListModel {
    Q_OBJECT

    Q_PROPERTY(AccountState* accountState READ accountState WRITE setAccountState NOTIFY accountStateChanged)
    Q_PROPERTY(QString localPath READ localPath WRITE setLocalPath NOTIFY fileChanged)
    Q_PROPERTY(QString fileIcon READ fileIcon NOTIFY fileChanged)
    Q_PROPERTY(QString responseLabel READ responseLabel WRITE setResponseLabel NOTIFY responseChanged)
    Q_PROPERTY(QString responseUrl READ responseUrl WRITE setResponseUrl NOTIFY responseChanged)

public:
    explicit FileActionsModel(QObject *const parent = nullptr);
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    enum DataRole {
        FileActionIconRole = Qt::UserRole + 1,
        FileActionNameRole,
        FileActionUrlRole,
        FileActionMethodRole,
        FileActionParamsRole
    };
    Q_ENUM(DataRole)

    struct Response {
        QString label;
        QString url;
    };

    [[nodiscard]] AccountState *accountState() const;
    void setAccountState(AccountState *accountState);

    [[nodiscard]] QString localPath() const;
    void setLocalPath(const QString &localPath);

    [[nodiscard]] QByteArray fileId() const;
    [[nodiscard]] QMimeType mimeType() const;
    [[nodiscard]] QString fileIcon() const;
    void setupFileProperties();

    [[nodiscard]] QString responseLabel() const;
    void setResponseLabel(const QString &label);

    [[nodiscard]] QString responseUrl() const;
    void setResponseUrl(const QString &url);

    void setResponse(const Response &response);

    void parseEndpoints();
    QString parseUrl(const QString &url) const;

signals:
    void accountStateChanged();
    void fileChanged();
    void responseChanged();
    void fileActionModelChanged();

public slots:
    void createRequest(const int row);
    void processRequest(const QJsonDocument &json, int statusCode);

private:
    Response _response;
    struct FileAction {
        QString icon;
        QString name;
        QString url;
        QString method;
        QList<QString> params;
    };
    QList<FileAction> _fileActions;
    AccountState *_accountState;
    QString _localPath;
    QByteArray _fileId;
    QMimeType _mimeType;
    QString _filePath;
    QString _accountUrl;
    QString _fileIcon;

    static constexpr char fileIdUrlC[] = "{fileId}";
    static constexpr char fileIdC[] = "fileId";
    static constexpr char filePathC[] = "filePath";
};

}
