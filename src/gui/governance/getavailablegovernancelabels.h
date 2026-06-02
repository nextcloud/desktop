/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef GETAVAILABLEGOVERNANCELABELS_H
#define GETAVAILABLEGOVERNANCELABELS_H

#include "typedgovernancenetworkjob.h"
#include <QObject>
#include <QQmlEngine>

namespace OCC
{

class GetAvailableGovernanceLabels : public OCC::TypedGovernanceNetworkJob
{
    Q_OBJECT
    QML_ELEMENT

public:
    explicit GetAvailableGovernanceLabels(QObject *parent = nullptr);

Q_SIGNALS:

private:
};

} // namespace OCC

#endif // GETAVAILABLEGOVERNANCELABELS_H
