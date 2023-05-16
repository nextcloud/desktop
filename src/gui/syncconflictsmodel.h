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

#pragma once

#include "tray/activitydata.h"

#include "conflictsolver.h"

#include <QAbstractListModel>
#include <QMimeDatabase>
#include <QLocale>

namespace OCC {

class SyncConflictsModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(OCC::ActivityList conflictActivities READ conflictActivities WRITE setConflictActivities NOTIFY conflictActivitiesChanged)

    Q_PROPERTY(bool allExistingsSelected READ allExistingsSelected NOTIFY allExistingsSelectedChanged)

    Q_PROPERTY(bool allConflictingSelected READ allConflictingSelected NOTIFY allConflictingSelectedChanged)

    struct ConflictInfo {
        enum class ConflictSolution  : bool{
            SolutionSelected = true,
            SolutionDeselected = false,
        };


        QString mExistingFileName;
        QString mExistingSize;
        QString mConflictSize;
        QString mExistingDate;
        QString mConflictDate;
        QUrl mExistingPreviewUrl;
        QUrl mConflictPreviewUrl;
        ConflictSolution mExistingSelected = ConflictSolution::SolutionDeselected;
        ConflictSolution mConflictSelected = ConflictSolution::SolutionDeselected;
        QString mExistingFilePath;
        QString mConflictingFilePath;

        [[nodiscard]] ConflictSolver::Solution solution() const;
        [[nodiscard]] bool isValid() const;
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

    [[nodiscard]] bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

    [[nodiscard]] QHash<int,QByteArray> roleNames() const override;

    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex &index) const override;

    [[nodiscard]] OCC::ActivityList conflictActivities() const;

    [[nodiscard]] bool allExistingsSelected() const;

    [[nodiscard]] bool allConflictingSelected() const;

public slots:
    void setConflictActivities(OCC::ActivityList conflicts);

    void selectAllExisting(bool selected);

    void selectAllConflicting(bool selected);

    void applySolution();

signals:
    void conflictActivitiesChanged();

    void allExistingsSelectedChanged();

    void allConflictingSelectedChanged();

private:
    void updateConflictsData();

    void setExistingSelected(bool value,
                             const QModelIndex &index,
                             int role);

    void setConflictingSelected(bool value,
                                const QModelIndex &index,
                                int role);

    OCC::ActivityList _data;

    QVector<ConflictInfo> _conflictData;

    QLocale _locale;

    bool _allExistingsSelected = false;

    bool _allConflictingsSelected = false;
};

}
