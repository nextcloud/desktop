/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QObject>

#include <qqmlintegration.h>

#include "permission.h"
#include "property.h"
#include "recipient.h"

#include "accountfwd.h"

namespace OCC::Gui::Sharing {

class Share : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("created via SharingController")

    Q_PROPERTY(QString id READ id NOTIFY idChanged)
    Q_PROPERTY(Share::ShareState state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString permissionPreset READ permissionPreset NOTIFY permissionPresetChanged)
    Q_PROPERTY(QList<QPointer<Permission>> permissions READ permissions NOTIFY permissionsChanged)
    Q_PROPERTY(QList<QPointer<Property>> properties READ properties NOTIFY propertiesChanged)
    Q_PROPERTY(QList<QPointer<Recipient>> recipients READ recipients NOTIFY recipientsChanged)

public:
    [[nodiscard]] static QPointer<Share> fromJson(const QJsonDocument &json, const AccountPtr &account);

    enum class ShareState {
        Draft,
        Active,
        Deleted
    };
    Q_ENUM(ShareState)

    void updateFromJson(const QJsonDocument &json);

    [[nodiscard]] QString id() const;
    [[nodiscard]] ShareState state() const;
    [[nodiscard]] QString permissionPreset() const;
    [[nodiscard]] const QList<QPointer<Permission>> &permissions() const;
    [[nodiscard]] const QList<QPointer<Property>> &properties() const;
    [[nodiscard]] const QList<QPointer<Recipient>> &recipients() const;

Q_SIGNALS:
    void idChanged();
    void stateChanged();
    void permissionPresetChanged();
    void permissionsChanged();
    void propertiesChanged();
    void recipientsChanged();

private:
    AccountPtr _account;
    QString _id;
    ShareState _state = ShareState::Draft;
    QString _permissionPreset;
    QList<QPointer<Permission>> _permissions;
    QList<QPointer<Property>> _properties;
    QList<QPointer<Recipient>> _recipients;

    explicit Share(const AccountPtr &account);

    void setId(const QString &id);
    void setState(const QString &state);
    void setPermissionPreset(const QString &permissionPreset);
    void setPermissions(const QJsonArray &permissions);
    void setProperties(const QJsonArray &properties);
    void setRecipients(const QJsonArray &recipients);
};

}
