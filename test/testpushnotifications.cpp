#include <QTest>
#include <QVector>
#include <QWebSocketServer>
#include <QSignalSpy>

#include "pushnotifications.h"
#include "pushnotificationstestutils.h"

class TestPushNotifications : public QObject
{
    Q_OBJECT

private slots:
    void testSetup_correctCredentials_authenticateAndEmitReady()
    {
        FakeWebSocketServer fakeServer;
        QSignalSpy processTextMessageSpy(&fakeServer, &FakeWebSocketServer::processTextMessage);
        QVERIFY(processTextMessageSpy.isValid());
        const QString user = "user";
        const QString password = "password";
        auto account = FakeWebSocketServer::createAccount();
        auto credentials = new CredentialsStub(user, password);
        account->setCredentials(credentials);
        QSignalSpy readySpy(account->pushNotifications(), &OCC::PushNotifications::ready);
        QVERIFY(readySpy.isValid());

        // Wait for authentication
        QVERIFY(processTextMessageSpy.wait());

        // Right authentication data should be sent
        QCOMPARE(processTextMessageSpy.count(), 2);

        const auto socket = processTextMessageSpy.at(0).at(0).value<QWebSocket *>();
        const auto userSent = processTextMessageSpy.at(0).at(1).toString();
        const auto passwordSent = processTextMessageSpy.at(1).at(1).toString();

        QCOMPARE(userSent, user);
        QCOMPARE(passwordSent, password);

        // Sent authenticated
        socket->sendTextMessage("authenticated");

        // Wait for ready signal
        readySpy.wait();
        QCOMPARE(readySpy.count(), 1);
        QCOMPARE(account->pushNotifications()->isReady(), true);
    }

    void testOnWebSocketTextMessageReceived_notifyFileMessage_emitFilesChanged()
    {
        const QString user = "user";
        const QString password = "password";
        FakeWebSocketServer fakeServer;
        QSignalSpy processTextMessageSpy(&fakeServer, &FakeWebSocketServer::processTextMessage);
        QVERIFY(processTextMessageSpy.isValid());

        auto account = FakeWebSocketServer::createAccount();
        auto credentials = new CredentialsStub(user, password);
        account->setCredentials(credentials);
        QSignalSpy filesChangedSpy(account->pushNotifications(), &OCC::PushNotifications::filesChanged);
        QVERIFY(filesChangedSpy.isValid());

        // Wait for authentication and then send notify_file push notification
        QVERIFY(processTextMessageSpy.wait());
        QCOMPARE(processTextMessageSpy.count(), 2);
        const auto socket = processTextMessageSpy.at(0).at(0).value<QWebSocket *>();
        socket->sendTextMessage("notify_file");

        // filesChanged signal should be emitted
        QVERIFY(filesChangedSpy.wait());
        QCOMPARE(filesChangedSpy.count(), 1);
        auto accountFilesChanged = filesChangedSpy.at(0).at(0).value<OCC::Account *>();
        QCOMPARE(accountFilesChanged, account.data());
    }

    void testOnWebSocketTextMessageReceived_notifyActivityMessage_emitNotification()
    {
        const QString user = "user";
        const QString password = "password";
        FakeWebSocketServer fakeServer;
        QSignalSpy processTextMessageSpy(&fakeServer, &FakeWebSocketServer::processTextMessage);
        QVERIFY(processTextMessageSpy.isValid());

        auto account = FakeWebSocketServer::createAccount();
        auto credentials = new CredentialsStub(user, password);
        account->setCredentials(credentials);
        QSignalSpy activitySpy(account->pushNotifications(), &OCC::PushNotifications::activitiesChanged);
        QVERIFY(activitySpy.isValid());

        // Wait for authentication and then send notify_file push notification
        QVERIFY(processTextMessageSpy.wait());
        QCOMPARE(processTextMessageSpy.count(), 2);
        const auto socket = processTextMessageSpy.at(0).at(0).value<QWebSocket *>();
        socket->sendTextMessage("notify_activity");

        // notification signal should be emitted
        QVERIFY(activitySpy.wait());
        QCOMPARE(activitySpy.count(), 1);
        auto accountFilesChanged = activitySpy.at(0).at(0).value<OCC::Account *>();
        QCOMPARE(accountFilesChanged, account.data());
    }

    void testOnWebSocketTextMessageReceived_notifyNotificationMessage_emitNotification()
    {
        const QString user = "user";
        const QString password = "password";
        FakeWebSocketServer fakeServer;
        QSignalSpy processTextMessageSpy(&fakeServer, &FakeWebSocketServer::processTextMessage);
        QVERIFY(processTextMessageSpy.isValid());

        auto account = FakeWebSocketServer::createAccount();
        auto credentials = new CredentialsStub(user, password);
        account->setCredentials(credentials);
        QSignalSpy notificationSpy(account->pushNotifications(), &OCC::PushNotifications::notificationsChanged);
        QVERIFY(notificationSpy.isValid());

        // Wait for authentication and then send notify_file push notification
        QVERIFY(processTextMessageSpy.wait());
        QCOMPARE(processTextMessageSpy.count(), 2);
        const auto socket = processTextMessageSpy.at(0).at(0).value<QWebSocket *>();
        socket->sendTextMessage("notify_notification");

        // notification signal should be emitted
        QVERIFY(notificationSpy.wait());
        QCOMPARE(notificationSpy.count(), 1);
        auto accountFilesChanged = notificationSpy.at(0).at(0).value<OCC::Account *>();
        QCOMPARE(accountFilesChanged, account.data());
    }

    void testOnWebSocketTextMessageReceived_invalidCredentialsMessage_reconnectWebSocket()
    {
        const QString user = "user";
        const QString password = "password";
        FakeWebSocketServer fakeServer;
        QSignalSpy processTextMessageSpy(&fakeServer, &FakeWebSocketServer::processTextMessage);
        QVERIFY(processTextMessageSpy.isValid());

        auto account = FakeWebSocketServer::createAccount();
        auto credentials = new CredentialsStub(user, password);
        account->setCredentials(credentials);
        // Need to set reconnect timer interval to zero for tests
        account->pushNotifications()->setReconnectTimerInterval(0);

        // Wait for authentication attempt and then sent invalid credentials
        QVERIFY(processTextMessageSpy.wait());
        QCOMPARE(processTextMessageSpy.count(), 2);
        const auto socket = processTextMessageSpy.at(0).at(0).value<QWebSocket *>();
        const auto firstPasswordSent = processTextMessageSpy.at(1).at(1).toString();
        QCOMPARE(firstPasswordSent, password);
        processTextMessageSpy.clear();
        socket->sendTextMessage("err: Invalid credentials");

        // Wait for a new authentication attempt
        QVERIFY(processTextMessageSpy.wait());
        QCOMPARE(processTextMessageSpy.count(), 2);
        const auto secondPasswordSent = processTextMessageSpy.at(1).at(1).toString();
        QCOMPARE(secondPasswordSent, password);
    }

    void testOnWebSocketError_connectionLost_emitConnectionLost()
    {
        const QString user = "user";
        const QString password = "password";
        FakeWebSocketServer fakeServer;
        QSignalSpy processTextMessageSpy(&fakeServer, &FakeWebSocketServer::processTextMessage);
        QVERIFY(processTextMessageSpy.isValid());

        auto account = FakeWebSocketServer::createAccount();
        auto credentials = new CredentialsStub(user, password);
        account->setCredentials(credentials);
        // Need to set reconnect timer interval to zero for tests
        account->pushNotifications()->setReconnectTimerInterval(0);

        QSignalSpy connectionLostSpy(account->pushNotifications(), &OCC::PushNotifications::connectionLost);
        QVERIFY(connectionLostSpy.isValid());

        // Wait for authentication and then sent a network error
        processTextMessageSpy.wait();
        QCOMPARE(processTextMessageSpy.count(), 2);
        auto socket = processTextMessageSpy.at(0).at(0).value<QWebSocket *>();
        socket->abort();

        QVERIFY(connectionLostSpy.wait());
        // Account handled connectionLost signal and deleted PushNotifications
        QCOMPARE(account->pushNotifications(), nullptr);
    }

    void testSetup_maxConnectionAttemptsReached_deletePushNotifications()
    {
        const QString user = "user";
        const QString password = "password";
        FakeWebSocketServer fakeServer;
        QSignalSpy processTextMessageSpy(&fakeServer, &FakeWebSocketServer::processTextMessage);
        QVERIFY(processTextMessageSpy.isValid());

        auto account = FakeWebSocketServer::createAccount();
        auto credentials = new CredentialsStub(user, password);
        account->setCredentials(credentials);
        account->pushNotifications()->setReconnectTimerInterval(0);
        QSignalSpy authenticationFailedSpy(account->pushNotifications(), &OCC::PushNotifications::authenticationFailed);
        QVERIFY(authenticationFailedSpy.isValid());

        // Let three authentication attempts fail
        QVERIFY(processTextMessageSpy.wait());
        QCOMPARE(processTextMessageSpy.count(), 2);
        auto socket = processTextMessageSpy.at(0).at(0).value<QWebSocket *>();
        socket->sendTextMessage("err: Invalid credentials");

        QVERIFY(processTextMessageSpy.wait());
        QCOMPARE(processTextMessageSpy.count(), 4);
        socket = processTextMessageSpy.at(2).at(0).value<QWebSocket *>();
        socket->sendTextMessage("err: Invalid credentials");

        QVERIFY(processTextMessageSpy.wait());
        QCOMPARE(processTextMessageSpy.count(), 6);
        socket = processTextMessageSpy.at(4).at(0).value<QWebSocket *>();
        socket->sendTextMessage("err: Invalid credentials");

        // Now the authenticationFailed Signal should be emitted
        QVERIFY(authenticationFailedSpy.wait());
        QCOMPARE(authenticationFailedSpy.count(), 1);

        // Account deleted the push notifications
        QCOMPARE(account->pushNotifications(), nullptr);
    }

    void testOnWebSocketSslError_sslError_deletePushNotifications()
    {
        const QString user = "user";
        const QString password = "password";
        FakeWebSocketServer fakeServer;
        QSignalSpy processTextMessageSpy(&fakeServer, &FakeWebSocketServer::processTextMessage);
        QVERIFY(processTextMessageSpy.isValid());

        auto account = FakeWebSocketServer::createAccount();
        auto credentials = new CredentialsStub(user, password);
        account->setCredentials(credentials);

        processTextMessageSpy.wait();

        // FIXME: This a little bit ugly but I had no better idea how to trigger a error on the websocket client.
        // The websocket that is retrived through the server is not connected to the ssl error signal.
        auto pushNotificationsWebSocketChildren = account->pushNotifications()->findChildren<QWebSocket *>();
        QVERIFY(pushNotificationsWebSocketChildren.size() == 1);
        emit pushNotificationsWebSocketChildren[0]->sslErrors(QList<QSslError>());

        // Account handled connectionLost signal and deleted PushNotifications
        QCOMPARE(account->pushNotifications(), nullptr);
    }

    void testAccountSetCredentials_correctCredentials_emitPushNotificationsReady()
    {
        FakeWebSocketServer fakeServer;
        auto account = FakeWebSocketServer::createAccount();
        QSignalSpy processTextMessageSpy(&fakeServer, &FakeWebSocketServer::processTextMessage);
        QVERIFY(processTextMessageSpy.isValid());
        const QString user = "user";
        const QString password = "password";
        auto credentials = new CredentialsStub(user, password);
        account->setCredentials(credentials);

        QSignalSpy pushNotificationsReady(account.data(), &OCC::Account::pushNotificationsReady);
        QVERIFY(pushNotificationsReady.isValid());

        // Wait for authentication
        QVERIFY(processTextMessageSpy.wait());
        auto socket = processTextMessageSpy.at(0).at(0).value<QWebSocket *>();
        // Don't care about which message was sent
        socket->sendTextMessage("authenticated");

        // Wait for push notifactions ready signal
        QVERIFY(pushNotificationsReady.wait());
        auto accountSent = pushNotificationsReady.at(0).at(0).value<OCC::Account *>();
        QCOMPARE(accountSent, account.data());
    }

    void testAccount_web_socket_connectionLost_emitNotificationsDisabled()
    {
        FakeWebSocketServer fakeServer;
        auto account = FakeWebSocketServer::createAccount();
        QSignalSpy processTextMessageSpy(&fakeServer, &FakeWebSocketServer::processTextMessage);
        QVERIFY(processTextMessageSpy.isValid());
        const QString user = "user";
        const QString password = "password";
        auto credentials = new CredentialsStub(user, password);
        account->setCredentials(credentials);

        // Need to set reconnect timer interval to zero for tests
        account->pushNotifications()->setReconnectTimerInterval(0);

        QSignalSpy connectionLostSpy(account->pushNotifications(), &OCC::PushNotifications::connectionLost);
        QVERIFY(connectionLostSpy.isValid());

        QSignalSpy pushNotificationsDisabledSpy(account.data(), &OCC::Account::pushNotificationsDisabled);
        QVERIFY(pushNotificationsDisabledSpy.isValid());

        // Wait for authentication and then sent a network error
        processTextMessageSpy.wait();
        QCOMPARE(processTextMessageSpy.count(), 2);
        auto socket = processTextMessageSpy.at(0).at(0).value<QWebSocket *>();
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
        QSignalSpy processTextMessageSpy(&fakeServer, &FakeWebSocketServer::processTextMessage);
        QVERIFY(processTextMessageSpy.isValid());
        const QString user = "user";
        const QString password = "password";
        auto credentials = new CredentialsStub(user, password);
        account->setCredentials(credentials);

        account->pushNotifications()->setReconnectTimerInterval(0);
        QSignalSpy authenticationFailedSpy(account->pushNotifications(), &OCC::PushNotifications::authenticationFailed);
        QVERIFY(authenticationFailedSpy.isValid());

        QSignalSpy pushNotificationsDisabledSpy(account.data(), &OCC::Account::pushNotificationsDisabled);
        QVERIFY(pushNotificationsDisabledSpy.isValid());

        // Let three authentication attempts fail
        QVERIFY(processTextMessageSpy.wait());
        QCOMPARE(processTextMessageSpy.count(), 2);
        auto socket = processTextMessageSpy.at(0).at(0).value<QWebSocket *>();
        socket->sendTextMessage("err: Invalid credentials");

        QVERIFY(processTextMessageSpy.wait());
        QCOMPARE(processTextMessageSpy.count(), 4);
        socket = processTextMessageSpy.at(2).at(0).value<QWebSocket *>();
        socket->sendTextMessage("err: Invalid credentials");

        QVERIFY(processTextMessageSpy.wait());
        QCOMPARE(processTextMessageSpy.count(), 6);
        socket = processTextMessageSpy.at(4).at(0).value<QWebSocket *>();
        socket->sendTextMessage("err: Invalid credentials");

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
