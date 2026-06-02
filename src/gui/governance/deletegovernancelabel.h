/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DELETEGOVERNANCELABEL_H
#define DELETEGOVERNANCELABEL_H

#include "typedgovernancenetworkjob.h"
#include <QObject>
#include <QQmlEngine>

namespace OCC
{

class DeleteGovernanceLabel : public OCC::TypedGovernanceNetworkJob
{
    Q_OBJECT
    QML_ELEMENT
public:
    explicit DeleteGovernanceLabel(QObject *parent = nullptr);
};

} // namespace OCC

#endif // DELETEGOVERNANCELABEL_H
