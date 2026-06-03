/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TYPEDWITHLABELIDGOVERNANCENETWORKJOB_H
#define TYPEDWITHLABELIDGOVERNANCENETWORKJOB_H

#include "typedgovernancenetworkjob.h"
#include <QObject>
#include <QQmlEngine>

namespace OCC
{

class TypedWithLabelIdGovernanceNetworkJob : public OCC::TypedGovernanceNetworkJob
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString labelId READ labelId WRITE setLabelId NOTIFY labelIdChanged FINAL)

public:
    explicit TypedWithLabelIdGovernanceNetworkJob(AccountPtr account,
                                                  QObject *parent = nullptr);

    [[nodiscard]] QString labelId() const;

    void setLabelId(const QString &newLabelId);

signals:
    void labelIdChanged();

protected:
    [[nodiscard]] QString buildPath() const override;

private:
    QString _labelId;
};

} // namespace OCC

#endif // TYPEDWITHLABELIDGOVERNANCENETWORKJOB_H
