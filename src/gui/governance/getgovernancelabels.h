/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef GETGOVERNANCELABELS_H
#define GETGOVERNANCELABELS_H

#include "governancenetworkjob.h"
#include <QObject>
#include <QQmlEngine>

namespace OCC
{

class GetGovernanceLabels : public OCC::GovernanceNetworkJob
{
    Q_OBJECT
    QML_ELEMENT
public:
    explicit GetGovernanceLabels(QObject *parent = nullptr);
};

} // namespace OCC

#endif // GETGOVERNANCELABELS_H
