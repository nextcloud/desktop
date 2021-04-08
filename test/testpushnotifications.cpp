#include <QTest>
#include <QVector>
#include <QWebSocketServer>
#include <QSignalSpy>

#include "accountfwd.h"
#include "pushnotifications.h"
#include "pushnotificationstestutils.h"

bool verifyCalledOnceWithAccount(QSignalSpy &spy, OCC::AccountPtr account)
{
    if (spy.count() != 1) {
        return false;
    }

    auto accountFromSpy = spy.at(0).at(0).value<OCC::Account *>();
    if (accountFromSpy != account.data()) {
        return false;
    }

    return true;
}

class TestPushNotifications : public QObject
{
    Q_OBJECT

private slots:
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
        QVERIFY(connectionLostSpy.isValid());

        // Wait for authentication and then sent a network error
        QVERIFY(fakeServer.waitForTextMessages());
        QCOMPARE(fakeServer.textMessagesCount(), 2);
        auto socket = fakeServer.socketForTextMessage(0);
        socket->abort();

        QVERIFY(connectionLostSpy.wait());
        // Account handled connectionLost signal and deleted PushNotifications
        QCOMPARE(account->pushNotifications(), nullptr);
    }

    void testSetup_maxConnectionAttemptsReached_deletePushNotifications()
    {
        FakeWebSocketServer fakeServer;
        auto account = FakeWebSocketServer::createAccount();
        account->pushNotifications()->setReconnectTimerInterval(0);
        QSignalSpy authenticationFailedSpy(account->pushNotifications(), &OCC::PushNotifications::authenticationFailed);
        QVERIFY(authenticationFailedSpy.isValid());

        // Let three authentication attempts fail
        for (uint8_t i = 0; i < 3; ++i) {
            QVERIFY(fakeServer.waitForTextMessages());
            QCOMPARE(fakeServer.textMessagesCount(), 2);
            auto socket = fakeServer.socketForTextMessage(0);
            fakeServer.clearTextMessages();
            socket->sendTextMessage("err: Invalid credentials");
        }

        // Now the authenticationFailed Signal should be emitted
        QVERIFY(authenticationFailedSpy.wait());
        QCOMPARE(authenticationFailedSpy.count(), 1);
        // Account deleted the push notifications
        QCOMPARE(account->pushNotifications(), nullptr);
    }

    void testOnWebSocketSslError_sslError_deletePushNotifications()
    {
        FakeWebSocketServer fakeServer;
        auto account = FakeWebSocketServer::createAccount();

        QVERIFY(fakeServer.waitForTextMessages());
        // FIXME: This a little bit ugly but I had no better idea how to trigger a error on the websocket client.
        // The websocket that is retrived through the server is not connected to the ssl error signal.
        auto pushNotificationsWebSocketChildren = account->pushNotifications()->findChildren<QWebSocket *>();
        QVERIFY(pushNotificationsWebSocketChildren.size() == 1);
        emit pushNotificationsWebSocketChildren[0]->sslErrors(QList<QSslError>());

        // Account handled connectionLost signal and deleted PushNotifications
        QCOMPARE(account->pushNotifications(), nullptr);
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
        account->pushNotifications()->setReconnectTimerInterval(0);
        QSignalSpy authenticationFailedSpy(account->pushNotifications(), &OCC::PushNotifications::authenticationFailed);
        QVERIFY(authenticationFailedSpy.isValid());
        QSignalSpy pushNotificationsDisabledSpy(account.data(), &OCC::Account::pushNotificationsDisabled);
        QVERIFY(pushNotificationsDisabledSpy.isValid());

        // Let three authentication attempts fail
        for (uint8_t i = 0; i < 3; ++i) {
            QVERIFY(fakeServer.waitForTextMessages());
            QCOMPARE(fakeServer.textMessagesCount(), 2);
            auto socket = fakeServer.socketForTextMessage(0);
            fakeServer.clearTextMessages();
            socket->sendTextMessage("err: Invalid credentials");
        }

        // Now the authenticationFailed and pushNotificationsDisabled Signals should be emitted
        QVERIFY(pushNotificationsDisabledSpy.wait());
        QCOMPARE(pushNotificationsDisabledSpy.count(), 1);
        QCOMPARE(authenticationFailedSpy.count(), 1);
        auto accountSent = pushNotificationsDisabledSpy.at(0).at(0).value<OCC::Account *>();
        QCOMPARE(accountSent, account.data());
    }
};

QTEST_GUILESS_MAIN(TestPushNotifications)
#include "testpushnotifications.moc"
