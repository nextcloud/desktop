/*
 * Copyright (C) by Camila <hello@camila.codes>
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

#ifndef USERSTATUS_H
#define USERSTATUS_H

#include <QPointer>
#include "accountfwd.h"

namespace OCC {

class JsonApiJob;

class UserStatus : public QObject
{
    Q_OBJECT
    
public:
    explicit UserStatus(QObject *parent = nullptr);
    enum class Status {
        Online,
        DoNotDisturb,
        Away,
        Offline,
        Invisible
    };
    Q_ENUM(Status);
    void fetchUserStatus(AccountPtr account);
    Status status() const;
    QString message() const;
    QUrl icon() const;

private slots:
    void slotFetchUserStatusFinished(const QJsonDocument &json, int statusCode);

signals:
    void fetchUserStatusFinished();

private:
    Status stringToEnum(const QString &status) const;
    QString enumToString(Status status) const;
    QPointer<JsonApiJob> _job; // the currently running job
    Status _status = Status::Online;
    QString _message;
};


} // namespace OCC

#endif //USERSTATUS_H
