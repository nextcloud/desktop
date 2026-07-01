/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef APPLYGOVERNANCELABEL_H
#define APPLYGOVERNANCELABEL_H

#include "typedwithlabelidgovernancenetworkjob.h"

#include <QObject>
#include <QQmlEngine>
#include <QJsonDocument>

namespace OCC
{

class ApplyGovernanceLabel : public OCC::TypedWithLabelIdGovernanceNetworkJob
{
    Q_OBJECT
    QML_ELEMENT
    Q_INTERFACES(QQmlParserStatus)

public:
    explicit ApplyGovernanceLabel(QObject *parent = nullptr);

    void classBegin() override;

    void componentComplete() override;

public Q_SLOTS:
    void start();

    void start(const QString &labelId);

protected:
    [[nodiscard]] QString buildPath() const override;

private Q_SLOTS:
    void jobDone(QJsonDocument reply, int statusCode);
};

} // namespace OCC

#endif // APPLYGOVERNANCELABEL_H
