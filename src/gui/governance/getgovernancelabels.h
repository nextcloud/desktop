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
    Q_INTERFACES(QQmlParserStatus)

public:
    explicit GetGovernanceLabels(QObject *parent = nullptr);

    void classBegin() override;

    void componentComplete() override;

public Q_SLOTS:
    void start();

    void start(const QString &entityId);

protected:
    [[nodiscard]] QString buildPath() const override;

private Q_SLOTS:
    void jobDone(QJsonDocument reply, int statusCode);
};

} // namespace OCC

#endif // GETGOVERNANCELABELS_H
