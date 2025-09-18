/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QTest>

#include "capabilities.h"
#include "logger.h"

#include <QStandardPaths>

class TestCapabilities : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);

        QStandardPaths::setTestModeEnabled(true);
    }

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

    void testUserStatus_userStatusAvailable_returnTrue()
    {
        QVariantMap userStatusMap;
        userStatusMap["enabled"] = true;

        QVariantMap capabilitiesMap;
        capabilitiesMap["user_status"] = userStatusMap;

        const OCC::Capabilities capabilities(capabilitiesMap);

        QVERIFY(capabilities.userStatus());
    }

    void testUserStatus_userStatusNotAvailable_returnFalse()
    {
        QVariantMap userStatusMap;
        userStatusMap["enabled"] = false;

        QVariantMap capabilitiesMap;
        capabilitiesMap["user_status"] = userStatusMap;

        const OCC::Capabilities capabilities(capabilitiesMap);

        QVERIFY(!capabilities.userStatus());
    }

    void testUserStatus_userStatusNotInCapabilites_returnFalse()
    {
        QVariantMap capabilitiesMap;

        const OCC::Capabilities capabilities(capabilitiesMap);

        QVERIFY(!capabilities.userStatus());
    }

    void testUserStatusSupportsEmoji_supportsEmojiAvailable_returnTrue()
    {
        QVariantMap userStatusMap;
        userStatusMap["enabled"] = true;
        userStatusMap["supports_emoji"] = true;

        QVariantMap capabilitiesMap;
        capabilitiesMap["user_status"] = userStatusMap;

        const OCC::Capabilities capabilities(capabilitiesMap);

        QVERIFY(capabilities.userStatus());
    }

    void testUserStatusSupportsEmoji_supportsEmojiNotAvailable_returnFalse()
    {
        QVariantMap userStatusMap;
        userStatusMap["enabled"] = true;
        userStatusMap["supports_emoji"] = false;

        QVariantMap capabilitiesMap;
        capabilitiesMap["user_status"] = userStatusMap;

        const OCC::Capabilities capabilities(capabilitiesMap);

        QVERIFY(!capabilities.userStatusSupportsEmoji());
    }

    void testUserStatusSupportsEmoji_supportsEmojiNotInCapabilites_returnFalse()
    {
        QVariantMap userStatusMap;
        userStatusMap["enabled"] = true;

        QVariantMap capabilitiesMap;
        capabilitiesMap["user_status"] = userStatusMap;

        const OCC::Capabilities capabilities(capabilitiesMap);

        QVERIFY(!capabilities.userStatusSupportsEmoji());
    }

    void testUserStatusSupportsBusy_supportsBusyAvailable_returnTrue()
    {
        QVariantMap userStatusMap;
        userStatusMap["enabled"] = true;
        userStatusMap["supports_busy"] = true;

        QVariantMap capabilitiesMap;
        capabilitiesMap["user_status"] = userStatusMap;

        const OCC::Capabilities capabilities(capabilitiesMap);

        QVERIFY(capabilities.userStatusSupportsBusy());
    }

    void testUserStatusSupportsBusy_supportsBusyNotAvailable_returnFalse()
    {
        QVariantMap userStatusMap;
        userStatusMap["enabled"] = true;
        userStatusMap["supports_busy"] = false;

        QVariantMap capabilitiesMap;
        capabilitiesMap["user_status"] = userStatusMap;

        const OCC::Capabilities capabilities(capabilitiesMap);

        QVERIFY(!capabilities.userStatusSupportsBusy());
    }

    void testUserStatusSupportsBusy_supportsBusyNotInCapabilities_returnFalse()
    {
        QVariantMap userStatusMap;
        userStatusMap["enabled"] = true;

        QVariantMap capabilitiesMap;
        capabilitiesMap["user_status"] = userStatusMap;

        const OCC::Capabilities capabilities(capabilitiesMap);

        QVERIFY(!capabilities.userStatusSupportsBusy());
    }

    void testUserStatusSupportsBusy_userStatusNotEnabled_returnFalse()
    {
        QVariantMap userStatusMap;
        userStatusMap["enabled"] = false;
        userStatusMap["supports_busy"] = true;

        QVariantMap capabilitiesMap;
        capabilitiesMap["user_status"] = userStatusMap;

        const OCC::Capabilities capabilities(capabilitiesMap);

        QVERIFY(!capabilities.userStatusSupportsBusy());
    }
    
    void testShareDefaultPermissions_defaultSharePermissionsNotInCapabilities_returnZero()
    {
        QVariantMap filesSharingMap;
        filesSharingMap["api_enabled"] = false;
        
        QVariantMap capabilitiesMap;
        capabilitiesMap["files_sharing"] = filesSharingMap;
        
        const OCC::Capabilities capabilities(capabilitiesMap);
        const auto defaultSharePermissionsNotInCapabilities = capabilities.shareDefaultPermissions();

        QCOMPARE(defaultSharePermissionsNotInCapabilities, {});
    }
    
    void testShareDefaultPermissions_defaultSharePermissionsAvailable_returnPermissions()
    {
        QVariantMap filesSharingMap;
        filesSharingMap["api_enabled"] = true;
        filesSharingMap["default_permissions"] = 31;
        
        QVariantMap capabilitiesMap;
        capabilitiesMap["files_sharing"] = filesSharingMap;
        
        const OCC::Capabilities capabilities(capabilitiesMap);
        const auto defaultSharePermissionsAvailable = capabilities.shareDefaultPermissions();

        QCOMPARE(defaultSharePermissionsAvailable, 31);
    }

    void testBulkUploadAvailable_bulkUploadAvailable_returnTrue()
    {
        QVariantMap bulkuploadMap;
        bulkuploadMap["bulkupload"] = "1.0";

        QVariantMap capabilitiesMap;
        capabilitiesMap["dav"] = bulkuploadMap;

        const auto &capabilities = OCC::Capabilities(capabilitiesMap);
        const auto bulkuploadAvailable = capabilities.bulkUpload();

        QCOMPARE(bulkuploadAvailable, true);
    }

    void testFilesLockAvailable_filesLockAvailable_returnTrue()
    {
        QVariantMap filesMap;
        filesMap["locking"] = "1.0";

        QVariantMap capabilitiesMap;
        capabilitiesMap["files"] = filesMap;

        const auto &capabilities = OCC::Capabilities(capabilitiesMap);
        const auto filesLockAvailable = capabilities.filesLockAvailable();

        QCOMPARE(filesLockAvailable, true);
    }

    void testSupport_hasValidSubscription_returnTrue()
    {
        QVariantMap supportMap;
        supportMap["hasValidSubscription"] = "true";

        QVariantMap capabilitiesMap;
        capabilitiesMap["support"] = supportMap;

        const auto &capabilities = OCC::Capabilities(capabilitiesMap);
        const auto serverHasValidSubscription = capabilities.serverHasValidSubscription();

        QCOMPARE(serverHasValidSubscription, true);
    }

    void testSupport_desktopEnterpriseChannel_returnString()
    {
        QVariantMap supportMap;
        const auto defaultChannel = "stable";
        supportMap["desktopEnterpriseChannel"] = defaultChannel;

        QVariantMap capabilitiesMap;
        capabilitiesMap["support"] = supportMap;

        const auto &capabilities = OCC::Capabilities(capabilitiesMap);
        const auto enterpriseChannel = capabilities.desktopEnterpriseChannel();

        QCOMPARE(enterpriseChannel, defaultChannel);
    }
};

QTEST_GUILESS_MAIN(TestCapabilities)
#include "testcapabilities.moc"
