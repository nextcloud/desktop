/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef GETAVAILABLEGOVERNANCELABELS_H
#define GETAVAILABLEGOVERNANCELABELS_H

#include "typedgovernancenetworkjob.h"

#include <QObject>
#include <QQmlEngine>
#include <QJsonDocument>

namespace OCC
{

class GetAvailableGovernanceLabels : public OCC::TypedGovernanceNetworkJob
{
    Q_OBJECT
    QML_ELEMENT

public:
    explicit GetAvailableGovernanceLabels(QObject *parent = nullptr);

    void start() override;

Q_SIGNALS:

public Q_SLOTS:
    void start(OCC::Governance::LabelType labelType, const QString &entityId);

protected:
    [[nodiscard]] QString buildPath() const override;
};

} // namespace OCC

#endif // GETAVAILABLEGOVERNANCELABELS_H
