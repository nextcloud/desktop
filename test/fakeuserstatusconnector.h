/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once
#include "userstatusconnector.h"

class FakeUserStatusConnector : public OCC::UserStatusConnector
{
public:
    void fetchUserStatus() override
    {
        if (_couldNotFetchUserStatus) {
            Q_EMIT error(Error::CouldNotFetchUserStatus);
            return;
        } else if (_userStatusNotSupported) {
            Q_EMIT error(Error::UserStatusNotSupported);
            return;
        } else if (_emojisNotSupported) {
            Q_EMIT error(Error::EmojisNotSupported);
            return;
        }

        Q_EMIT userStatusFetched(_userStatus);
    }

    void fetchPredefinedStatuses() override
    {
        if (_couldNotFetchPredefinedUserStatuses) {
            Q_EMIT error(Error::CouldNotFetchPredefinedUserStatuses);
            return;
        }
        Q_EMIT predefinedStatusesFetched(_predefinedStatuses);
    }

    [[nodiscard]] bool setUserStatus(const OCC::UserStatus &userStatus) override
    {
        ++_setUserStatusCallCount;
        if (_rejectUserStatusChanges) {
            return false;
        }

        if (_couldNotSetUserStatusMessage) {
            Q_EMIT error(Error::CouldNotSetUserStatus);
            return false;
        }

        _userStatusSetByCallerOfSetUserStatus = userStatus;
        if (!_deferUserStatusSetSignal) {
            Q_EMIT UserStatusConnector::userStatusSet();
        }
        return true;
    }

    void clearMessage() override
    {
        if (_couldNotClearUserStatusMessage) {
            Q_EMIT error(Error::CouldNotClearMessage);
        } else {
            _isMessageCleared = true;
            Q_EMIT UserStatusConnector::messageCleared();
        }
    }

    [[nodiscard]] OCC::UserStatus userStatus() const override
    {
        return {}; // Not implemented
    }

    [[nodiscard]] bool supportsBusyStatus() const override
    {
        return true;
    }

    void setFakeUserStatus(const OCC::UserStatus &userStatus)
    {
        _userStatus = userStatus;
    }

    void setFakePredefinedStatuses(const QVector<OCC::UserStatus> &statuses)
    {
        _predefinedStatuses = statuses;
    }

    [[nodiscard]] OCC::UserStatus userStatusSetByCallerOfSetUserStatus() const
    {
        return _userStatusSetByCallerOfSetUserStatus;
    }
    [[nodiscard]] int setUserStatusCallCount() const
    {
        return _setUserStatusCallCount;
    }

    [[nodiscard]] bool messageCleared() const
    {
        return _isMessageCleared;
    }

    void emitUserStatusSet()
    {
        Q_EMIT UserStatusConnector::userStatusSet();
    }

    void emitError(Error error)
    {
        Q_EMIT UserStatusConnector::error(error);
    }

    void setErrorCouldNotFetchPredefinedUserStatuses(bool value)
    {
        _couldNotFetchPredefinedUserStatuses = value;
    }

    void setErrorCouldNotFetchUserStatus(bool value)
    {
        _couldNotFetchUserStatus = value;
    }

    void setErrorCouldNotSetUserStatusMessage(bool value)
    {
        _couldNotSetUserStatusMessage = value;
    }

    void setErrorUserStatusNotSupported(bool value)
    {
        _userStatusNotSupported = value;
    }

    void setErrorEmojisNotSupported(bool value)
    {
        _emojisNotSupported = value;
    }

    void setErrorCouldNotClearUserStatusMessage(bool value)
    {
        _couldNotClearUserStatusMessage = value;
    }

    void setRejectUserStatusChanges(bool value)
    {
        _rejectUserStatusChanges = value;
    }

    void setDeferUserStatusSetSignal(bool value)
    {
        _deferUserStatusSetSignal = value;
    }

private:
    OCC::UserStatus _userStatusSetByCallerOfSetUserStatus;
    OCC::UserStatus _userStatus;
    QVector<OCC::UserStatus> _predefinedStatuses;
    int _setUserStatusCallCount = 0;
    bool _isMessageCleared = false;
    bool _couldNotFetchPredefinedUserStatuses = false;
    bool _couldNotFetchUserStatus = false;
    bool _couldNotSetUserStatusMessage = false;
    bool _userStatusNotSupported = false;
    bool _emojisNotSupported = false;
    bool _couldNotClearUserStatusMessage = false;
    bool _rejectUserStatusChanges = false;
    bool _deferUserStatusSetSignal = false;
};
