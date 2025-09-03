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
    Q_PROPERTY(QString declarativeUiName READ name WRITE setName NOTIFY responseChanged)
    Q_PROPERTY(QString declarativeUiType READ type WRITE setType NOTIFY responseChanged)
    Q_PROPERTY(QString declarativeUiLabel READ type WRITE setLabel NOTIFY responseChanged)
    Q_PROPERTY(QString declarativeUiUrl READ url WRITE setUrl NOTIFY responseChanged)
    Q_PROPERTY(QString declarativeUiText READ text WRITE setText NOTIFY responseChanged)

public:
    explicit EndpointModel(QObject *const parent = nullptr);
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    enum DataRole {
        EndpointTypeRole = Qt::UserRole + 1,
        EndpointNameRole,
        EndpointUrlRole,
        EndpointIconRole,
        EndpointFilterRole,
        EndpointParameterRole,
        EndpointVerbRole
    };
    Q_ENUM(DataRole)

    struct Response {
        QString name;
        QString type;
        QString label;
        QString url;
        QString text;
    };

    void parseEndpoints();

    void setAccountState(AccountState *accountState);
    void setLocalPath(const QString &localPath);
    void setResponse(const Response &response);

    [[nodiscard]] AccountState *accountState() const;
    [[nodiscard]] QString localPath() const;

    [[nodiscard]] QString name() const;
    void setName(const QString &name);

    [[nodiscard]] QString type() const;
    void setType(const QString &type);

    [[nodiscard]] QString label() const;
    void setLabel(const QString &label);

    [[nodiscard]] QString url() const;
    void setUrl(const QString &url);

    [[nodiscard]] QString text() const;
    void setText(const QString &text);


signals:
    void endpointModelChanged();
    void localPathChanged();
    void accountStateChanged();
    void responseChanged();

public slots:
    void createRequest(const int row);
    void processRequest(const QJsonDocument &json);

private:
    Response _response;

    struct Endpoint {
        QString type;
        QString name;
        QString url;
        QString icon;
        QString filter;
        QString parameter;
        QString verb;
    };
    QList<Endpoint> _endpoints;
    AccountState *_accountState;
    QString _localPath;
};

}
