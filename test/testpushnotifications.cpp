/*
 * Copyright (C) by Felix Weilbach <felix.weilbach@nextcloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include <QTest>
#include <QVector>
#include <QWebSocketServer>
#include <QSignalSpy>

#include "accountfwd.h"
#include "pushnotifications.h"
#include "pushnotificationstestutils.h"
#include "logger.h"

#include <QStandardPaths>

#define RETURN_FALSE_ON_FAIL(expr) \
    if (!(expr)) {                 \
        return false;              \
    }

bool verifyCalledOnceWithAccount(QSignalSpy &spy, OCC::AccountPtr account)
{
    RETURN_FALSE_ON_FAIL(spy.count() == 1);
    auto accountFromSpy = spy.at(0).at(0).value<OCC::Account *>();
    RETURN_FALSE_ON_FAIL(accountFromSpy == account.data());

    return true;
}

bool failThreeAuthenticationAttempts(FakeWebSocketServer &fakeServer, OCC::AccountPtr account)
{
    RETURN_FALSE_ON_FAIL(account);
    RETURN_FALSE_ON_FAIL(account->pushNotifications());

    account->pushNotifications()->setReconnectTimerInterval(0);

    QSignalSpy authenticationFailedSpy(account->pushNotifications(), &OCC::PushNotifications::authenticationFailed);

    // Let three authentication attempts fail
    for (uint8_t i = 0; i < 3; ++i) {
        RETURN_FALSE_ON_FAIL(fakeServer.waitForTextMessages());
        RETURN_FALSE_ON_FAIL(fakeServer.textMessagesCount() == 2);
        auto socket = fakeServer.socketForTextMessage(0);
        fakeServer.clearTextMessages();
        socket->sendTextMessage("err: Invalid credentials");
    }

    // Now the authenticationFailed Signal should be emitted
    RETURN_FALSE_ON_FAIL(authenticationFailedSpy.wait());
    RETURN_FALSE_ON_FAIL(authenticationFailedSpy.count() == 1);

    return true;
}

class TestPushNotifications : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);

        QStandardPaths::setTestModeEnabled(true);
    }

    void testTryReconnect_capabilitesReportPushNotificationsAvailable_reconnectForEver()
    {
        FakeWebSocketServer fakeServer;
        auto account = FakeWebSocketServer::createAccount();

        // Let if fail a few times
        QVERIFY(failThreeAuthenticationAttempts(fakeServer, account));
        account->pushNotifications()->setup();
        QVERIFY(failThreeAuthenticationAttempts(fakeServer, account));

        account->setPushNotificationsReconnectInterval(0);

        // Push notifications should try to reconnect
        QVERIFY(fakeServer.authenticateAccount(account));

        account->setPushNotificationsReconnectInterval(1000 * 60 * 2);
    }

    void testSetup_correctCredentials_authenticateAndEmitReady()
    {
        FakeWebSocketServer fakeServer;
        std::unique_ptr<QSignalSpy> filesChangedSpy;
        std::unique_ptr<QSignalSpy> notificationsChangedSpy;
        std::unique_ptr<QSignalSpy> activitiesChangedSpy;
        auto account = FakeWebSocketServer::createAccount();

        QVERIFY(fakeServer.authenticateAccount(
            account, [&](OCC::PushNotifications *pushNotifications) {
                filesChangedSpy.reset(new QSignalSpy(pushNotifications, &OCC::PushNotifications::filesChanged));
                notificationsChangedSpy.reset(new QSignalSpy(pushNotifications, &OCC::PushNotifications::notificationsChanged));
                activitiesChangedSpy.reset(new QSignalSpy(pushNotifications, &OCC::PushNotifications::activitiesChanged));
            },
            [&] {
                QVERIFY(verifyCalledOnceWithAccount(*filesChangedSpy, account));
                QVERIFY(verifyCalledOnceWithAccount(*notificationsChangedSpy, account));
                QVERIFY(verifyCalledOnceWithAccount(*activitiesChangedSpy, account));
            }));
    }

    void testOnWebSocketTextMessageReceived_notifyFileMessage_emitFilesChanged()
    {
        FakeWebSocketServer fakeServer;
        auto account = FakeWebSocketServer::createAccount();
        const auto socket = fakeServer.authenticateAccount(account);
        QVERIFY(socket);
        QSignalSpy filesChangedSpy(account->pushNotifications(), &OCC::PushNotifications::filesChanged);

        socket->sendTextMessage("notify_file");

        // filesChanged signal should be emitted
        QVERIFY(filesChangedSpy.wait());
        QVERIFY(verifyCalledOnceWithAccount(filesChangedSpy, account));
    }

    void testOnWebSocketTextMessageReceived_notifyActivityMessage_emitNotification()
    {
        FakeWebSocketServer fakeServer;
        auto account = FakeWebSocketServer::createAccount();
        const auto socket = fakeServer.authenticateAccount(account);
        QVERIFY(socket);
        QSignalSpy activitySpy(account->pushNotifications(), &OCC::PushNotifications::activitiesChanged);
        QVERIFY(activitySpy.isValid());

        // Send notify_file push notification
        socket->sendTextMessage("notify_activity");

        // notification signal should be emitted
        QVERIFY(activitySpy.wait());
        QVERIFY(verifyCalledOnceWithAccount(activitySpy, account));
    }

    void testOnWebSocketTextMessageReceived_notifyNotificationMessage_emitNotification()
    {
        FakeWebSocketServer fakeServer;
        auto account = FakeWebSocketServer::createAccount();
        const auto socket = fakeServer.authenticateAccount(account);
        QVERIFY(socket);
        QSignalSpy notificationSpy(account->pushNotifications(), &OCC::PushNotifications::notificationsChanged);
        QVERIFY(notificationSpy.isValid());

        // Send notify_file push notification
        socket->sendTextMessage("notify_notification");

        // notification signal should be emitted
        QVERIFY(notificationSpy.wait());
        QVERIFY(verifyCalledOnceWithAccount(notificationSpy, account));
    }

    void testOnWebSocketTextMessageReceived_invalidCredentialsMessage_reconnectWebSocket()
    {
        FakeWebSocketServer fakeServer;
        auto account = FakeWebSocketServer::createAccount();
        // Need to set reconnect timer interval to zero for tests
        account->pushNotifications()->setReconnectTimerInterval(0);

        // Wait for authentication attempt and then sent invalid credentials
        QVERIFY(fakeServer.waitForTextMessages());
        QCOMPARE(fakeServer.textMessagesCount(), 2);
        const auto socket = fakeServer.socketForTextMessage(0);
        const auto firstPasswordSent = fakeServer.textMessage(1);
        QCOMPARE(firstPasswordSent, account->credentials()->password());
        fakeServer.clearTextMessages();
        socket->sendTextMessage("err: Invalid credentials");

        // Wait for a new authentication attempt
        QVERIFY(fakeServer.waitForTextMessages());
        QCOMPARE(fakeServer.textMessagesCount(), 2);
        const auto secondPasswordSent = fakeServer.textMessage(1);
        QCOMPARE(secondPasswordSent, account->credentials()->password());
    }

    void testOnWebSocketError_connectionLost_emitConnectionLost()
    {
        FakeWebSocketServer fakeServer;
        auto account = FakeWebSocketServer::createAccount();
        QSignalSpy connectionLostSpy(account->pushNotifications(), &OCC::PushNotifications::connectionLost);
        QSignalSpy pushNotificationsDisabledSpy(account.data(), &OCC::Account::pushNotificationsDisabled);
        QVERIFY(connectionLostSpy.isValid());

        // Wait for authentication and then sent a network error
        QVERIFY(fakeServer.waitForTextMessages());
        QCOMPARE(fakeServer.textMessagesCount(), 2);
        auto socket = fakeServer.socketForTextMessage(0);
        socket->abort();

        QVERIFY(connectionLostSpy.wait());
        // Account handled connectionLost signal and disabled push notifications
        QCOMPARE(pushNotificationsDisabledSpy.count(), 1);
    }

    void testSetup_maxConnectionAttemptsReached_disablePushNotifications()
    {
        FakeWebSocketServer fakeServer;
        auto account = FakeWebSocketServer::createAccount();
        QSignalSpy pushNotificationsDisabledSpy(account.data(), &OCC::Account::pushNotificationsDisabled);

        QVERIFY(failThreeAuthenticationAttempts(fakeServer, account));
        // Account disabled the push notifications
        QCOMPARE(pushNotificationsDisabledSpy.count(), 1);
    }

    void testOnWebSocketSslError_sslError_disablePushNotifications()
    {
        FakeWebSocketServer fakeServer;
        auto account = FakeWebSocketServer::createAccount();
        QSignalSpy pushNotificationsDisabledSpy(account.data(), &OCC::Account::pushNotificationsDisabled);

        QVERIFY(fakeServer.waitForTextMessages());
        // FIXME: This a little bit ugly but I had no better idea how to trigger a error on the websocket client.
        // The websocket that is retrieved through the server is not connected to the ssl error signal.
        auto pushNotificationsWebSocketChildren = account->pushNotifications()->findChildren<QWebSocket *>();
        QVERIFY(pushNotificationsWebSocketChildren.size() == 1);
        emit pushNotificationsWebSocketChildren[0]->sslErrors(QList<QSslError>());

        // Account handled connectionLost signal and the authenticationFailed Signal should be emitted
        QCOMPARE(pushNotificationsDisabledSpy.count(), 1);
    }

    void testAccount_web_socket_connectionLost_emitNotificationsDisabled()
    {
        FakeWebSocketServer fakeServer;
        auto account = FakeWebSocketServer::createAccount();
        // Need to set reconnect timer interval to zero for tests
        account->pushNotifications()->setReconnectTimerInterval(0);
        const auto socket = fakeServer.authenticateAccount(account);
        QVERIFY(socket);

        QSignalSpy connectionLostSpy(account->pushNotifications(), &OCC::PushNotifications::connectionLost);
        QVERIFY(connectionLostSpy.isValid());

        QSignalSpy pushNotificationsDisabledSpy(account.data(), &OCC::Account::pushNotificationsDisabled);
        QVERIFY(pushNotificationsDisabledSpy.isValid());

        // Wait for authentication and then sent a network error
        socket->abort();

        QVERIFY(pushNotificationsDisabledSpy.wait());
        QCOMPARE(pushNotificationsDisabledSpy.count(), 1);

        QCOMPARE(connectionLostSpy.count(), 1);

        auto accountSent = pushNotificationsDisabledSpy.at(0).at(0).value<OCC::Account *>();
        QCOMPARE(accountSent, account.data());
    }

    void testAccount_web_socket_authenticationFailed_emitNotificationsDisabled()
    {
        FakeWebSocketServer fakeServer;
        auto account = FakeWebSocketServer::createAccount();
        QSignalSpy pushNotificationsDisabledSpy(account.data(), &OCC::Account::pushNotificationsDisabled);
        QVERIFY(pushNotificationsDisabledSpy.isValid());

        QVERIFY(failThreeAuthenticationAttempts(fakeServer, account));

        // Now the pushNotificationsDisabled Signal should be emitted
        QCOMPARE(pushNotificationsDisabledSpy.count(), 1);
        auto accountSent = pushNotificationsDisabledSpy.at(0).at(0).value<OCC::Account *>();
        QCOMPARE(accountSent, account.data());
    }

    void testPingTimeout_pingTimedOut_reconnect()
    {
        FakeWebSocketServer fakeServer;
        std::unique_ptr<QSignalSpy> filesChangedSpy;
        std::unique_ptr<QSignalSpy> notificationsChangedSpy;
        std::unique_ptr<QSignalSpy> activitiesChangedSpy;
        auto account = FakeWebSocketServer::createAccount();
        QVERIFY(fakeServer.authenticateAccount(account));

        // Set the ping timeout interval to zero and check if the server attempts to authenticate again
        fakeServer.clearTextMessages();
        account->pushNotifications()->setPingInterval(0);
        QVERIFY(fakeServer.authenticateAccount(
            account, [&](OCC::PushNotifications *pushNotifications) {
                filesChangedSpy.reset(new QSignalSpy(pushNotifications, &OCC::PushNotifications::filesChanged));
                notificationsChangedSpy.reset(new QSignalSpy(pushNotifications, &OCC::PushNotifications::notificationsChanged));
                activitiesChangedSpy.reset(new QSignalSpy(pushNotifications, &OCC::PushNotifications::activitiesChanged));
            },
            [&] {
                QVERIFY(verifyCalledOnceWithAccount(*filesChangedSpy, account));
                QVERIFY(verifyCalledOnceWithAccount(*notificationsChangedSpy, account));
                QVERIFY(verifyCalledOnceWithAccount(*activitiesChangedSpy, account));
            }));
    }
};

QTEST_GUILESS_MAIN(TestPushNotifications)
#include "testpushnotifications.moc"
