/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QLoggingCategory>
#include <QtCore>

#include "accountstate.h"

namespace OCC {

class EndpointModel : public QAbstractListModel {
    Q_OBJECT

    Q_PROPERTY(AccountState* accountState READ accountState WRITE setAccountState NOTIFY accountStateChanged)
    Q_PROPERTY(QString localPath READ localPath WRITE setLocalPath NOTIFY localPathChanged)
    Q_PROPERTY(QString responseLabel READ label WRITE setLabel NOTIFY responseChanged)
    Q_PROPERTY(QString responseUrl READ url WRITE setUrl NOTIFY responseChanged)

public:
    explicit EndpointModel(QObject *const parent = nullptr);
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    enum DataRole {
        EndpointIconRole = Qt::UserRole + 1,
        EndpointNameRole,
        EndpointUrlRole,
        EndpointMethodRole,
        EndpointParamsRole
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
    void setFileId();

    [[nodiscard]] QMimeType mimeType() const;
    void setMimeType();

    [[nodiscard]] QString label() const;
    void setLabel(const QString &label);

    [[nodiscard]] QString url() const;
    void setUrl(const QString &url);

    void setResponse(const Response &response);

    void parseEndpoints();
    QString parseUrl(const QString &url) const;

signals:
    void accountStateChanged();
    void localPathChanged();
    void responseChanged();
    void endpointModelChanged();

public slots:
    void createRequest(const int row);
    void processRequest(const QJsonDocument &json, int statusCode);

private:
    Response _response;

    struct Endpoint {
        QString icon;
        QString name;
        QString url;
        QString method;
        QString params;
    };
    QList<Endpoint> _endpoints;
    AccountState *_accountState;
    QString _localPath;
    QByteArray _fileId;
    QMimeType _mimeType;
};

}
