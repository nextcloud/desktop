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

public Q_SLOTS:
    void start();

protected:
    [[nodiscard]] QString buildPath() const override;

private Q_SLOTS:
    void jobDone(QJsonDocument reply, int statusCode);

    void initialize();
};

} // namespace OCC

#endif // GETGOVERNANCELABELS_H
