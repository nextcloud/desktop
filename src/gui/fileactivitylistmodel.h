/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
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
