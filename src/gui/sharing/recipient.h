/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QPointer>

#include <QtQmlIntegration>

#include <memory>

#include "permission.h"

#include <optional>

namespace OCC::Gui::Sharing {

/**
 * @brief A recipient attached to a unified share.
 */
class Recipient : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("created by SharingController")
    Q_PROPERTY(QString className READ className CONSTANT)
    Q_PROPERTY(QString displayName READ displayName CONSTANT)
    Q_PROPERTY(QString value READ value CONSTANT)
    Q_PROPERTY(QString instance READ instanceString CONSTANT)
    Q_PROPERTY(bool secretUpdatable READ secretUpdatable CONSTANT)
    Q_PROPERTY(QString secretUrl READ secretUrlString CONSTANT)
    Q_PROPERTY(bool hasPermissionData READ hasPermissionData NOTIFY permissionsChanged)
    Q_PROPERTY(QList<QPointer<Permission>> permissions READ permissions NOTIFY permissionsChanged)

public:
    /** @brief Creates a recipient from its unified sharing API representation and returns its owning pointer. */
    [[nodiscard]] static std::unique_ptr<Recipient> fromJson(const QJsonObject &json);
    /** @brief Updates this recipient from its unified sharing API representation. */
    void updateFromJson(const QJsonObject &json);

    /** @brief Returns the registered server class identifying the recipient type. */
    [[nodiscard]] QString className() const;
    /** @brief Returns the user-facing recipient name. */
    [[nodiscard]] QString displayName() const;
    /** @brief Returns the recipient identifier understood by its type. */
    [[nodiscard]] QString value() const;
    /** @brief Returns the recipient's remote instance, or no value for a local recipient. */
    [[nodiscard]] const std::optional<QString> &instance() const;
    [[nodiscard]] QString instanceString() const;
    /** @brief Returns a themeable SVG icon supplied by the server, if present. */
    [[nodiscard]] QString iconSvg() const;
    /** @brief Returns the light-theme icon URL supplied by the server, if present. */
    [[nodiscard]] QString iconLight() const;
    /** @brief Returns the dark-theme icon URL supplied by the server, if present. */
    [[nodiscard]] QString iconDark() const;
    /** @brief Returns whether the server allows this recipient's secret to be replaced. */
    [[nodiscard]] bool secretUpdatable() const;
    /** @brief Returns the public secret value, when the recipient type exposes it. */
    [[nodiscard]] const std::optional<QString> &secretValue() const;
    /** @brief Returns the public URL associated with the recipient secret, when exposed. */
    [[nodiscard]] const std::optional<QString> &secretUrl() const;
    [[nodiscard]] QString secretUrlString() const;
    /** @brief Returns the user-facing name of the user who added the recipient. */
    [[nodiscard]] QString initiatorDisplayName() const;
    /** @brief Returns the permissions currently assigned to this recipient. */
    [[nodiscard]] const QList<QPointer<Permission>> &permissions() const;
    /** @brief Returns whether the server supplied recipient-specific permissions. */
    [[nodiscard]] bool hasPermissionData() const;
    /** @brief Returns the recipient-specific permission override for a class, if one is available. */
    [[nodiscard]] std::optional<bool> permissionOverride(const QString &className) const;
    /** @brief Tracks a successfully applied recipient-specific permission update. */
    void setPermissionOverride(const QString &className, bool enabled);

Q_SIGNALS:
    void permissionsChanged();

private:
    explicit Recipient(QObject *parent = nullptr);

    QString _className;
    QString _displayName;
    QString _value;
    std::optional<QString> _instance;
    QString _iconSvg;
    QString _iconLight;
    QString _iconDark;
    bool _secretUpdatable = false;
    std::optional<QString> _secretValue;
    std::optional<QString> _secretUrl;
    QString _initiatorDisplayName;
    QList<QPointer<Permission>> _permissions;
    bool _hasPermissionData = false;
    QHash<QString, bool> _permissionOverrides;
};

}
