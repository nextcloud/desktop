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
    Q_PROPERTY(QString localPath READ localPath WRITE setLocalPath NOTIFY localPathChanged)

public:
    explicit FileActivityListModel(QObject *parent = nullptr);

    [[nodiscard]] QString localPath() const;

signals:
    void localPathChanged();

public slots:
    void setLocalPath(const QString &localPath);
    void load();

protected slots:
    void startFetchJob() override;

private:
    int _objectId = -1;
    QString _localPath;
};
}
