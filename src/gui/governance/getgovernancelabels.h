/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef GETGOVERNANCELABELS_H
#define GETGOVERNANCELABELS_H

#include "governancenetworkjob.h"

#include <QObject>
#include <QQmlEngine>
#include <QJsonDocument>

namespace OCC
{

class GetGovernanceLabels : public OCC::GovernanceNetworkJob
{
    Q_OBJECT
    QML_ELEMENT

public:
    explicit GetGovernanceLabels(QObject *parent = nullptr);

    void start() override;

public Q_SLOTS:
    void start(const QString &entityId);

protected:
    [[nodiscard]] QString buildPath() const override;
};

} // namespace OCC

#endif // GETGOVERNANCELABELS_H
