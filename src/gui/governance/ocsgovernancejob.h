/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef OCSGOVERNANCEJOB_H
#define OCSGOVERNANCEJOB_H

#include <QObject>
#include <ocsjob.h>

namespace OCC
{

class OcsGovernanceJob : public OCC::OcsJob
{
    Q_OBJECT
public:
    explicit OcsGovernanceJob(AccountPtr account);

    void setMethod(const QByteArray &method);

    void start() override;
};

} // namespace OCC

#endif // OCSGOVERNANCEJOB_H
