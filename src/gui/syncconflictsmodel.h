/*
 * Copyright (C) 2023 by Matthieu Gallien <matthieu.gallien@nextcloud.com>
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

#ifndef SYNCCONFLICTSMODEL_H
#define SYNCCONFLICTSMODEL_H

#include "tray/activitydata.h"

#include <QAbstractListModel>
#include <QMimeDatabase>
#include <QLocale>

namespace OCC {

class SyncConflictsModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(OCC::ActivityList conflictActivities READ conflictActivities WRITE setConflictActivities NOTIFY conflictActivitiesChanged)

    struct ConflictInfo {
        QString mExistingFileName;
        QString mExistingSize;
        QString mConflictSize;
        QString mExistingDate;
        QString mConflictDate;
        QUrl mExistingPreviewUrl;
        QUrl mConflictPreviewUrl;
        bool mExistingSelected = false;
        bool mConflictSelected = false;
    };

public:
    enum class SyncConflictRoles : int {
        ExistingFileName = Qt::UserRole,
        ExistingSize,
        ConflictSize,
        ExistingDate,
        ConflictDate,
        ExistingSelected,
        ConflictSelected,
        ExistingPreviewUrl,
        ConflictPreviewUrl,
    };

    Q_ENUM(SyncConflictRoles)

    explicit SyncConflictsModel(QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    [[nodiscard]] QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    [[nodiscard]] QHash<int,QByteArray> roleNames() const override;

    [[nodiscard]] OCC::ActivityList conflictActivities() const;

public slots:
    void setConflictActivities(OCC::ActivityList conflicts);

signals:
    void conflictActivitiesChanged();

private:
    void updateConflictsData();

    OCC::ActivityList mData;

    QVector<ConflictInfo> mConflictData;

    QMimeDatabase mMimeDb;

    QLocale mLocale;
};

}

#endif // SYNCCONFLICTSMODEL_H
