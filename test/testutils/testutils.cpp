#include "testutils.h"

#include "common/checksums.h"
#include "gui/accountmanager.h"
#include "libsync/creds/httpcredentials.h"

#include <QCoreApplication>
#include <QRandomGenerator>

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
        auto acc = Account::create(QUuid::createUuid());
        HttpCredentialsTest *cred = new HttpCredentialsTest(QStringLiteral("testuser"), QStringLiteral("secret"));
        acc->setCredentials(cred);
        acc->setUrl(QUrl(QStringLiteral("http://localhost/owncloud")));
        acc->setDavDisplayName(QStringLiteral("fakename") + acc->uuid().toString(QUuid::WithoutBraces));
        acc->setCapabilities(OCC::TestUtils::testCapabilities());
        OCC::AccountManager::instance()->addAccount(acc);
        return acc;
    }

    FolderDefinition createDummyFolderDefinition(const AccountPtr &account, const QString &path)
    {
        // TODO: legacy
        auto d = OCC::FolderDefinition::createNewFolderDefinition(account->davUrl());
        d.setLocalPath(path);
        d.setTargetPath(path);
        return d;
    }

    QTemporaryDir createTempDir()
    {
        return QTemporaryDir { QStringLiteral("%1/ownCloud-unit-test-%2-XXXXXX").arg(QDir::tempPath(), qApp->applicationName()) };
    }

    FolderMan *folderMan()
    {
        static QPointer<FolderMan> man;
        if (!man) {
            man = new FolderMan;
            QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, man, &FolderMan::deleteLater);
        };
        return man;
    }


    bool writeRandomFile(const QString &fname, int size)
    {
        auto rg = QRandomGenerator::global();

        const int maxSize = 10 * 10 * 1024;
        if (size == -1) {
            size = static_cast<int>(rg->generate() % maxSize);
        }

        QString randString;
        for (int i = 0; i < size; i++) {
            int r = static_cast<int>(rg->generate() % 128);
            randString.append(QChar(r));
        }

        QFile file(fname);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << randString;
            // optional, as QFile destructor will already do it:
            file.close();
            return true;
        }
        return false;
    }

    const QVariantMap testCapabilities(CheckSums::Algorithm algo)
    {
        static const auto algorithmNames = [] {
            QVariantList out;
            for (const auto &a : CheckSums::All) {
                out.append(CheckSums::toQString(a.first));
            }
            return out;
        }();
        return {
            { "core", QVariantMap { { "status", QVariantMap { { "installed", "1" }, { "maintenance", "0" }, { "needsDbUpgrade", "0" }, { "version", "10.11.0.0" }, { "versionstring", "10.11.0" }, { "edition", "Community" }, { "productname", "Infinite Scale" }, { "product", "Infinite Scale" }, { "productversion", "2.0.0-beta1+7c2e3201b" } } } } },
            { "files", QVariantList {} },
            { "dav", QVariantMap { { "chunking", "1.0" } } },
            { "checksums", QVariantMap { { "preferredUploadType", CheckSums::toQString(algo) }, { "supportedTypes", algorithmNames } } }
        };
    }
}
}
