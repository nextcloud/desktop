#include <QTest>

#include "tray/notificationcache.h"

class TestNotificationCache : public QObject
{
    Q_OBJECT

private slots:
    void testContains_doesNotContainNotification_returnsFalse()
    {
        OCC::NotificationCache notificationCache;

        QVERIFY(!notificationCache.contains({ "Title", { "Message" } }));
    }

    void testContains_doesContainNotification_returnTrue()
    {
        OCC::NotificationCache notificationCache;
        const OCC::NotificationCache::Notification notification { "Title", "message" };

        notificationCache.insert(notification);

        QVERIFY(notificationCache.contains(notification));
    }

    void testClear_doesContainNotification_clearNotifications()
    {
        OCC::NotificationCache notificationCache;
        const OCC::NotificationCache::Notification notification { "Title", "message" };

        notificationCache.insert(notification);
        notificationCache.clear();

        QVERIFY(!notificationCache.contains(notification));
    }
};

QTEST_GUILESS_MAIN(TestNotificationCache)
#include "testnotificationcache.moc"
