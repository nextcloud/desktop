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

#include <QNetworkReply>
#include <QObject>
#include <QString>

#include "accountstate.h"

namespace OCC::Mac {

class FileProviderEditLocallyJob;
using FileProviderEditLocallyJobPtr = QSharedPointer<FileProviderEditLocallyJob>;

class FileProviderEditLocallyJob : public QObject
{
    Q_OBJECT

public:
    explicit FileProviderEditLocallyJob(const AccountStatePtr &accountState,
                                        const QString &relPath,
                                        QObject * const parent = nullptr);

public slots:
    void start();

signals:
    void error(const QString &message, const QString &informativeText);
    void ocIdAcquired(const QString &ocId);
    void notAvailable();
    void finished();

private slots:
    void showError(const QString &message, const QString &informativeText);
    void idGetError(const QNetworkReply *const reply);
    void idGetFinished(const QVariantMap &data);
    void openFileProviderFile(const QString &ocId);

private:
    AccountStatePtr _accountState;
    QString _relPath;
};

}
