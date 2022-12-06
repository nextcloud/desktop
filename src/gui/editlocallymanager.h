/*
 * Copyright (C) by Claudio Cambra <claudio.cambra@nextcloud.com>
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

#include <QObject>
#include <QHash>

#include "editlocallyjob.h"

namespace OCC {

class EditLocallyManager : public QObject
{
    Q_OBJECT

public:
    [[nodiscard]] static EditLocallyManager *instance();

public slots:
    void editLocally(const QUrl &url);

private slots:
    void createJob(const QString &userId,
                   const QString &relPath,
                   const QString &token);

private:
    explicit EditLocallyManager(QObject *parent = nullptr);
    static EditLocallyManager *_instance;

    struct EditLocallyInputData {
        QString userId;
        QString relPath;
        QString token;
    };

    [[nodiscard]] static EditLocallyInputData parseEditLocallyUrl(const QUrl &url);

    QHash<QString, EditLocallyJobPtr> _jobs;
};

}
