// SPDX-FileCopyrightText: 2024 Claudio Cambra <claudio.cambra@nextcloud.com>
// SPDX-License-Identifier: GPL-2.0-or-later

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
