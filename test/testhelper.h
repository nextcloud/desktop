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

public slots:
    void checkConnectivity() override {};

private slots:
    void setState(OCC::AccountState::State state) override { Q_UNUSED(state) };
};


const QByteArray jsonValueToOccReply(const QJsonValue &jsonValue);

#endif // TESTHELPER_H
