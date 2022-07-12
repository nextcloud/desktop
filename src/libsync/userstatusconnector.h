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

#include "common/result.h"
#include "owncloudlib.h"

#include <QObject>
#include <QString>
#include <QMetaType>
#include <QUrl>
#include <QDateTime>
#include <QtGlobal>
#include <QVariant>

#include <vector>


namespace OCC {

enum class OWNCLOUDSYNC_EXPORT ClearAtType {
    Period,
    EndOf,
    Timestamp
};

// TODO: If we can use C++17 make it a std::variant
struct OWNCLOUDSYNC_EXPORT ClearAt
{
    ClearAtType _type = ClearAtType::Period;

    quint64 _timestamp;
    int _period;
    QString _endof;
};

class OWNCLOUDSYNC_EXPORT UserStatus
{
    Q_GADGET

    Q_PROPERTY(QString id MEMBER _id)
    Q_PROPERTY(QString message MEMBER _message)
    Q_PROPERTY(QString icon MEMBER _icon)
    Q_PROPERTY(OnlineStatus state MEMBER _state)

public:
    enum class OnlineStatus : quint8 {
        Online,
        DoNotDisturb,
        Away,
        Offline,
        Invisible
    };
    Q_ENUM(OnlineStatus);

    UserStatus();

    UserStatus(const QString &id, const QString &message, const QString &icon,
        OnlineStatus state, bool messagePredefined, const Optional<ClearAt> &clearAt = {});

    Q_REQUIRED_RESULT QString id() const;
    Q_REQUIRED_RESULT QString message() const;
    Q_REQUIRED_RESULT QString icon() const;
    Q_REQUIRED_RESULT OnlineStatus state() const;
    Q_REQUIRED_RESULT Optional<ClearAt> clearAt() const;

    QString clearAtDisplayString() const;

    void setId(const QString &id);
    void setMessage(const QString &message);
    void setState(OnlineStatus state);
    void setIcon(const QString &icon);
    void setMessagePredefined(bool value);
    void setClearAt(const Optional<ClearAt> &dateTime);

    Q_REQUIRED_RESULT bool messagePredefined() const;

    Q_REQUIRED_RESULT QUrl stateIcon() const;

private:
    QString _id;
    QString _message;
    QString _icon;
    OnlineStatus _state = OnlineStatus::Online;
    bool _messagePredefined;
    Optional<ClearAt> _clearAt;
};

class OWNCLOUDSYNC_EXPORT UserStatusConnector : public QObject
{
    Q_OBJECT

public:
    enum class Error {
        CouldNotFetchUserStatus,
        CouldNotFetchPredefinedUserStatuses,
        UserStatusNotSupported,
        EmojisNotSupported,
        CouldNotSetUserStatus,
        CouldNotClearMessage
    };
    Q_ENUM(Error)

    explicit UserStatusConnector(QObject *parent = nullptr);

    ~UserStatusConnector() override;

    virtual void fetchUserStatus() = 0;

    virtual void fetchPredefinedStatuses() = 0;

    virtual void setUserStatus(const UserStatus &userStatus) = 0;

    virtual void clearMessage() = 0;

    virtual UserStatus userStatus() const = 0;

signals:
    void userStatusFetched(const UserStatus &userStatus);
    void predefinedStatusesFetched(const QVector<UserStatus> &statuses);
    void userStatusSet();
    void serverUserStatusChanged();
    void messageCleared();
    void error(Error error);
};
}

Q_DECLARE_METATYPE(OCC::UserStatusConnector *)
Q_DECLARE_METATYPE(OCC::UserStatus)
