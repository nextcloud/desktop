/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
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

    quint64 _timestamp = 0ULL;
    int _period = 0;
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
        Busy,
        Offline,
        Invisible
    };
    Q_ENUM(OnlineStatus);

    UserStatus();

    UserStatus(const QString &id, const QString &message, const QString &icon,
        OnlineStatus state, bool messagePredefined, const Optional<ClearAt> &clearAt = {});

    [[nodiscard]] QString id() const;
    [[nodiscard]] QString message() const;
    [[nodiscard]] QString icon() const;
    [[nodiscard]] OnlineStatus state() const;
    [[nodiscard]] Optional<ClearAt> clearAt() const;

    [[nodiscard]] QString clearAtDisplayString() const;

    void setId(const QString &id);
    void setMessage(const QString &message);
    void setState(OnlineStatus state);
    void setIcon(const QString &icon);
    void setMessagePredefined(bool value);
    void setClearAt(const Optional<ClearAt> &dateTime);

    [[nodiscard]] bool messagePredefined() const;

    [[nodiscard]] QUrl stateIcon() const;

private:
    QString _id;
    QString _message;
    QString _icon;
    OnlineStatus _state = OnlineStatus::Online;
    bool _messagePredefined = false;
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

    [[nodiscard]] virtual UserStatus userStatus() const = 0;

    [[nodiscard]] virtual bool supportsBusyStatus() const = 0;

signals:
    void userStatusFetched(const OCC::UserStatus &userStatus);
    void predefinedStatusesFetched(const QVector<OCC::UserStatus> &statuses);
    void userStatusSet();
    void serverUserStatusChanged();
    void messageCleared();
    void error(OCC::UserStatusConnector::Error error);
};
}

Q_DECLARE_METATYPE(OCC::UserStatusConnector *)
Q_DECLARE_METATYPE(OCC::UserStatus)
