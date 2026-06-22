/*
 * SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QObject>

#include "accountstate.h"

namespace OCC {

class EditLocallyVerificationJob;
using EditLocallyVerificationJobPtr = QSharedPointer<EditLocallyVerificationJob>;

class EditLocallyVerificationJob : public QObject
{
    Q_OBJECT

public:
    explicit EditLocallyVerificationJob(const AccountStatePtr &accountState,
                                        const QString &relPath,
                                        const QString &token,
                                        QObject *const parent = nullptr);

    [[nodiscard]] static bool isTokenValid(const QString &token);
    [[nodiscard]] static bool isRelPathValid(const QString &relPath);

signals:
    void error(const QString &message, const QString &informativeText);
    void finished();

public slots:
    void start();

private slots:
    void responseReceived(const int statusCode);

private:
    AccountStatePtr _accountState;
    QString _relPath; // full remote path for a file (as on the server)
    QString _token;
};

}
