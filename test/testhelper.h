/*
 * SPDX-FileCopyrightText: 2019 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TESTHELPER_H
#define TESTHELPER_H

#include "gui/accountstate.h"
#include "gui/folder.h"
#include "creds/httpcredentials.h"

class HttpCredentialsTest : public OCC::HttpCredentials
{
public:
    HttpCredentialsTest(const QString& user, const QString& password)
    : HttpCredentials(user, password)
    {}

    void askFromUser() override {

    }
};

OCC::FolderDefinition folderDefinition(const QString &path);

class FakeAccountState : public OCC::AccountState
{
    Q_OBJECT

public:
    explicit FakeAccountState(OCC::AccountPtr account)
        : OCC::AccountState()
    {
        _account = account;
        _state = Connected;
    }

    static OCC::RemoteWipe *remoteWipe(OCC::AccountState *accountState)
    {
        return accountState->_remoteWipe;
    }

public slots:
    void checkConnectivity() override {};

    void setStateForTesting(OCC::AccountState::State state)
    {
        if (_state == state) {
            return;
        }

        _state = state;
        Q_EMIT stateChanged(state);
    }

private slots:
    void setState(OCC::AccountState::State state) override { Q_UNUSED(state) };
};


const QByteArray jsonValueToOccReply(const QJsonValue &jsonValue);

#endif // TESTHELPER_H
