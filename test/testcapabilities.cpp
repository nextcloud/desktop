#include <QTest>

#include "capabilities.h"

class TestCapabilities : public QObject
{
    Q_OBJECT

private slots:
    void testPushNotificationsAvailable_pushNotificationsForActivitiesAvailable_returnTrue()
    {
        QStringList typeList;
        typeList.append("activities");

        QVariantMap notifyPushMap;
        notifyPushMap["type"] = typeList;

        QVariantMap capabilitiesMap;
        capabilitiesMap["notify_push"] = notifyPushMap;

        const auto &capabilities = OCC::Capabilities(capabilitiesMap);
        const auto activitiesPushNotificationsAvailable = capabilities.availablePushNotifications().testFlag(OCC::PushNotificationType::Activities);

        QCOMPARE(activitiesPushNotificationsAvailable, true);
    }

    void testPushNotificationsAvailable_pushNotificationsForActivitiesNotAvailable_returnFalse()
    {
        QStringList typeList;
        typeList.append("noactivities");

        QVariantMap notifyPushMap;
        notifyPushMap["type"] = typeList;

        QVariantMap capabilitiesMap;
        capabilitiesMap["notify_push"] = notifyPushMap;

        const auto &capabilities = OCC::Capabilities(capabilitiesMap);
        const auto activitiesPushNotificationsAvailable = capabilities.availablePushNotifications().testFlag(OCC::PushNotificationType::Activities);

        QCOMPARE(activitiesPushNotificationsAvailable, false);
    }

    void testPushNotificationsAvailable_pushNotificationsForFilesAvailable_returnTrue()
    {
        QStringList typeList;
        typeList.append("files");

        QVariantMap notifyPushMap;
        notifyPushMap["type"] = typeList;

        QVariantMap capabilitiesMap;
        capabilitiesMap["notify_push"] = notifyPushMap;

        const auto &capabilities = OCC::Capabilities(capabilitiesMap);
        const auto filesPushNotificationsAvailable = capabilities.availablePushNotifications().testFlag(OCC::PushNotificationType::Files);

        QCOMPARE(filesPushNotificationsAvailable, true);
    }

    void testPushNotificationsAvailable_pushNotificationsForFilesNotAvailable_returnFalse()
    {
        QStringList typeList;
        typeList.append("nofiles");

        QVariantMap notifyPushMap;
        notifyPushMap["type"] = typeList;

        QVariantMap capabilitiesMap;
        capabilitiesMap["notify_push"] = notifyPushMap;

        const auto &capabilities = OCC::Capabilities(capabilitiesMap);
        const auto filesPushNotificationsAvailable = capabilities.availablePushNotifications().testFlag(OCC::PushNotificationType::Files);

        QCOMPARE(filesPushNotificationsAvailable, false);
    }

    void testPushNotificationsAvailable_pushNotificationsForNotificationsAvailable_returnTrue()
    {
        QStringList typeList;
        typeList.append("notifications");

        QVariantMap notifyPushMap;
        notifyPushMap["type"] = typeList;

        QVariantMap capabilitiesMap;
        capabilitiesMap["notify_push"] = notifyPushMap;

        const auto &capabilities = OCC::Capabilities(capabilitiesMap);
        const auto notificationsPushNotificationsAvailable = capabilities.availablePushNotifications().testFlag(OCC::PushNotificationType::Notifications);

        QCOMPARE(notificationsPushNotificationsAvailable, true);
    }

    void testPushNotificationsAvailable_pushNotificationsForNotificationsNotAvailable_returnFalse()
    {
        QStringList typeList;
        typeList.append("nonotifications");

        QVariantMap notifyPushMap;
        notifyPushMap["type"] = typeList;

        QVariantMap capabilitiesMap;
        capabilitiesMap["notify_push"] = notifyPushMap;

        const auto &capabilities = OCC::Capabilities(capabilitiesMap);
        const auto notificationsPushNotificationsAvailable = capabilities.availablePushNotifications().testFlag(OCC::PushNotificationType::Notifications);

        QCOMPARE(notificationsPushNotificationsAvailable, false);
    }

    void testPushNotificationsAvailable_pushNotificationsNotAvailable_returnFalse()
    {
        const auto &capabilities = OCC::Capabilities(QVariantMap());
        const auto activitiesPushNotificationsAvailable = capabilities.availablePushNotifications().testFlag(OCC::PushNotificationType::Activities);
        const auto filesPushNotificationsAvailable = capabilities.availablePushNotifications().testFlag(OCC::PushNotificationType::Files);
        const auto notificationsPushNotificationsAvailable = capabilities.availablePushNotifications().testFlag(OCC::PushNotificationType::Notifications);

        QCOMPARE(activitiesPushNotificationsAvailable, false);
        QCOMPARE(filesPushNotificationsAvailable, false);
        QCOMPARE(notificationsPushNotificationsAvailable, false);
    }

    void testPushNotificationsWebSocketUrl_urlAvailable_returnUrl()
    {
        QString websocketUrl("testurl");

        QVariantMap endpointsMap;
        endpointsMap["websocket"] = websocketUrl;

        QVariantMap notifyPushMap;
        notifyPushMap["endpoints"] = endpointsMap;

        QVariantMap capabilitiesMap;
        capabilitiesMap["notify_push"] = notifyPushMap;

        const auto &capabilities = OCC::Capabilities(capabilitiesMap);

        QCOMPARE(capabilities.pushNotificationsWebSocketUrl(), websocketUrl);
    }
};

QTEST_GUILESS_MAIN(TestCapabilities)
#include "testcapabilities.moc"
