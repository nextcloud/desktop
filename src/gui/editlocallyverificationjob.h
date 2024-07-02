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
