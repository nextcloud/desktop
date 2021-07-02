#include "testutils.h"

#include "creds/httpcredentials.h"
#include "gui/accountmanager.h"

#include <QCoreApplication>

namespace {
class HttpCredentialsTest : public OCC::HttpCredentials
{
public:
    HttpCredentialsTest(const QString &user, const QString &password)
        : HttpCredentials(OCC::DetermineAuthTypeJob::AuthType::Basic, user, password)
    {
    }

    void askFromUser() override
    {
    }
};
}

namespace OCC {

namespace TestUtils {
    AccountPtr createDummyAccount()
    {
        // don't use the account manager to create the account, it would try to use widgets
        auto acc = Account::create();
        HttpCredentialsTest *cred = new HttpCredentialsTest("testuser", "secret");
        acc->setCredentials(cred);
        acc->setUrl(QUrl(QStringLiteral("http://localhost/owncloud")));
        acc->setDavDisplayName(QStringLiteral("fakename") + acc->uuid().toString());
        acc->setServerVersion(QStringLiteral("10.0.0"));
        OCC::AccountManager::instance()->addAccount(acc);
        return acc;
    }

    FolderDefinition createDummyFolderDefinition(const QString &path)
    {
        OCC::FolderDefinition d;
        d.localPath = path;
        d.targetPath = path;
        d.alias = path;
        return d;
    }

    FolderMan *folderMan()
    {
        static FolderMan *man = [] {
            auto man = new FolderMan;
            QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, man, &FolderMan::deleteLater);
            return man;
        }();
        return man;
    }

}
}
