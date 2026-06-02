/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TYPEDGOVERNANCENETWORKJOB_H
#define TYPEDGOVERNANCENETWORKJOB_H

#include "governancenetworkjob.h"
#include <QObject>
#include <QQmlEngine>

namespace OCC
{

class TypedGovernanceNetworkJob : public GovernanceNetworkJob
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(LabelType labelType READ labelType WRITE setLabelType NOTIFY labelTypeChanged FINAL)

public:
    TypedGovernanceNetworkJob(QObject *parent = nullptr);

    [[nodiscard]] LabelType labelType() const;

    void setLabelType(LabelType newLabelType);

Q_SIGNALS:
    void labelTypeChanged();

private:
    LabelType _labelType;
};

} // namespace OCC

#endif // TYPEDGOVERNANCENETWORKJOB_H
