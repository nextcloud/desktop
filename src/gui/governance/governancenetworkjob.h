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
public:
    enum class EntityType {
        Files,
        Mails,
        Custom,
    };

    Q_ENUM(EntityType)

    enum class LabelType {
        Sensitivity,
        REtention,
        Hold,
    };

    Q_ENUM(LabelType)

    enum class ApiVersion {
        Version_1,
    };

    Q_ENUM(ApiVersion)

    explicit GovernanceNetworkJob(QObject *parent = nullptr);
};

} // namespace OCC

#endif // GOVERNANCENETWORKJOB_H
