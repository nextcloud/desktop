
/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#include "gui/models/activitylistmodel.h"
#include "gui/accountmanager.h"

#include <QTest>
#include <QAbstractItemModelTester>

namespace OCC {

class TestActivityModel : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testInsert()
    {
        auto model = new ActivityListModel(this);

        new QAbstractItemModelTester(model, this);

        auto manager = AccountManager::instance();

        auto createAcc = [&] {
            // don't use the account manager to create the account, it would try to use widgets
            auto acc = Account::create();
            acc->setUrl(QUrl(QStringLiteral("http://admin:admin@localhost/owncloud")));
            acc->setDavDisplayName(QStringLiteral("fakename") + acc->uuid().toString());
            acc->setServerVersion(QStringLiteral("10.0.0"));
            manager->addAccount(acc);
            return acc;
        };
        auto acc1 = createAcc();
        auto acc2 = createAcc();

        model->setActivityList({
            Activity { Activity::ActivityType, 1, acc1, "test", "test", "foo.cpp", QUrl::fromUserInput("https://owncloud.com"), QDateTime::currentDateTime() },
            Activity { Activity::ActivityType, 2, acc1, "test", "test", "foo.cpp", QUrl::fromUserInput("https://owncloud.com"), QDateTime::currentDateTime() },
            Activity { Activity::ActivityType, 4, acc2, "test", "test", "foo.cpp", QUrl::fromUserInput("https://owncloud.com"), QDateTime::currentDateTime() },
        });
        model->setActivityList({
            Activity { Activity::ActivityType, 1, acc2, "test", "test", "foo.cpp", QUrl::fromUserInput("https://owncloud.com"), QDateTime::currentDateTime() },
            Activity { Activity::ActivityType, 2, acc1, "test", "test", "foo.cpp", QUrl::fromUserInput("https://owncloud.com"), QDateTime::currentDateTime() },
            Activity { Activity::ActivityType, 4, acc2, "test", "test", "foo.cpp", QUrl::fromUserInput("https://owncloud.com"), QDateTime::currentDateTime() },
        });
        model->slotRemoveAccount(manager->accounts().first().data());
    }
};
}

QTEST_GUILESS_MAIN(OCC::TestActivityModel)
#include "testactivitymodel.moc"
