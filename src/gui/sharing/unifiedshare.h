/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QJsonArray>
#include <QJsonDocument>
#include <QObject>

#include <QtQmlIntegration>

#include <memory>
#include <vector>

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
    Q_PROPERTY(Share::State state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString permissionPreset READ permissionPreset NOTIFY permissionPresetChanged)
    Q_PROPERTY(QString permissionPresetLabel READ permissionPresetLabel NOTIFY permissionPresetChanged)
    Q_PROPERTY(QList<Permission *> permissions READ permissions NOTIFY permissionsChanged)
    Q_PROPERTY(QList<Property *> properties READ properties NOTIFY propertiesChanged)
    Q_PROPERTY(QList<Recipient *> recipients READ recipients NOTIFY recipientsChanged)
    Q_PROPERTY(bool publicLink READ isPublicLink NOTIFY recipientsChanged)
    Q_PROPERTY(QString publicLinkUrl READ publicLinkUrl NOTIFY recipientsChanged)

public:
    /** @brief Parses a share and returns its owning pointer. */
    [[nodiscard]] static std::unique_ptr<Share> fromJson(const QJsonDocument &json, const AccountPtr &account);

    enum class State {
        Unknown,
        Draft,
        Active,
        Deleted
    };
    Q_ENUM(State)

    void updateFromJson(const QJsonDocument &json);

    [[nodiscard]] QString id() const;
    [[nodiscard]] State state() const;
    [[nodiscard]] QString permissionPreset() const;
    /** @brief Returns the localized label for the known permission preset, or an empty string for custom or unknown presets. */
    [[nodiscard]] QString permissionPresetLabel() const;
    [[nodiscard]] QList<Permission *> permissions() const;
    [[nodiscard]] QList<Property *> properties() const;
    [[nodiscard]] QList<Recipient *> recipients() const;
    /** @brief Returns whether this share has the server's public-link recipient type. */
    [[nodiscard]] bool isPublicLink() const;
    /** @brief Returns the public URL exposed by the public-link recipient, if available. */
    [[nodiscard]] QString publicLinkUrl() const;

Q_SIGNALS:
    void idChanged();
    void stateChanged();
    void permissionPresetChanged();
    void permissionsChanged();
    void propertiesChanged();
    void recipientsChanged();

private:
    explicit Share(const AccountPtr &account);

    void setId(const QString &id);
    void setState(const QString &state);
    void setPermissionPreset(const QString &permissionPreset);
    void setPermissions(const QJsonArray &permissions);
    void setProperties(const QJsonArray &properties);
    void setRecipients(const QJsonArray &recipients);

    AccountPtr _account;
    QString _id;
    State _state = State::Unknown;
    QString _permissionPreset;
    std::vector<std::unique_ptr<Permission>> _permissions;
    std::vector<std::unique_ptr<Property>> _properties;
    std::vector<std::unique_ptr<Recipient>> _recipients;
};

}
