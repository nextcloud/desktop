/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef GOVERNANCENETWORKJOB_H
#define GOVERNANCENETWORKJOB_H

#include <QObject>
#include <QQmlEngine>

namespace OCC
{

class GovernanceNetworkJob : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(ApiVersion apiVersion READ apiVersion WRITE setApiVersion NOTIFY apiVersionChanged FINAL)

    Q_PROPERTY(EntityType entityType READ entityType WRITE setEntityType NOTIFY entityTypeChanged FINAL)

    Q_PROPERTY(QString customEntityType READ customEntityType WRITE setCustomEntityType NOTIFY customEntityTypeChanged FINAL)

    Q_PROPERTY(QString entityId READ entityId WRITE setEntityId NOTIFY entityIdChanged FINAL)

public:
    enum class EntityType {
        Files,
        Mails,
        Custom,
    };

    Q_ENUM(EntityType)

    enum class LabelType {
        Sensitivity,
        Retention,
        Hold,
    };

    Q_ENUM(LabelType)

    enum class ApiVersion {
        Version_1,
    };

    Q_ENUM(ApiVersion)

    explicit GovernanceNetworkJob(QObject *parent = nullptr);

    [[nodiscard]] ApiVersion apiVersion() const;

    void setApiVersion(ApiVersion newApiVersion);

    [[nodiscard]] EntityType entityType() const;

    void setEntityType(EntityType newEntityType);

    [[nodiscard]] QString customEntityType() const;

    void setCustomEntityType(const QString &newCustomEntityType);

    QString entityId() const;
    void setEntityId(const QString &newEntityId);

Q_SIGNALS:
    void apiVersionChanged();

    void entityTypeChanged();

    void customEntityTypeChanged();

    void entityIdChanged();

private:
    ApiVersion _apiVersion = ApiVersion::Version_1;

    EntityType _entityType = EntityType::Files;

    QString _customEntityType;

    QString _entityId;
};

} // namespace OCC

#endif // GOVERNANCENETWORKJOB_H
