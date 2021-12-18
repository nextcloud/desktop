/*
 * Copyright (C) by Felix Weilbach <felix.weilbach@nextcloud.com>
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

#include "accountstate.h"
#include "tray/activitylistmodel.h"

namespace OCC {

class FileActivityListModel : public ActivityListModel
{
    Q_OBJECT

public:
    explicit FileActivityListModel(QObject *parent = nullptr);

public slots:
    void load(AccountState *accountState, const QString &fileId);

protected:
    void startFetchJob() override;

private:
    QString _fileId;
};
}
