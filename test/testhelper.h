#ifndef TESTHELPER_H
#define TESTHELPER_H

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

#endif // TESTHELPER_H
