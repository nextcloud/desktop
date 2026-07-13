/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DELETEGOVERNANCELABEL_H
#define DELETEGOVERNANCELABEL_H

#include "typedwithlabelidgovernancenetworkjob.h"

#include <QObject>
#include <QQmlEngine>
#include <QJsonDocument>

namespace OCC
{

class DeleteGovernanceLabel : public OCC::TypedWithLabelIdGovernanceNetworkJob
{
    Q_OBJECT
    QML_ELEMENT

public:
    explicit DeleteGovernanceLabel(QObject *parent = nullptr);

    void start() override;

public Q_SLOTS:
    void start(const QString &labelId);

protected:
    [[nodiscard]] QString buildPath() const override;
};

} // namespace OCC

#endif // DELETEGOVERNANCELABEL_H
