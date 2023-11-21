/*
 * Copyright (C) 2023 by Oleksandr Zolotov <alex@nextcloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */
#pragma once

#include "ocsjob.h"

#include <QJsonDocument>
#include <QVariant>

namespace OCC {

/**
 * @brief The OcsClientStatusReportingJob class
 * @ingroup gui
 *
 * Handle sending client status reports via OCS Diagnostics API.
 */
class OcsClientStatusReportingJob : public OcsJob
{
    Q_OBJECT
public:
    explicit OcsClientStatusReportingJob(AccountPtr account);
    void sendStatusReport(const QVariant &jsonData);

signals:
    void jobFinished(QJsonDocument reply, QVariant value);

private slots:
    void jobDone(QJsonDocument reply);
};
}
