/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "gui/accountmanager.h"
#include "gui/creds/webflowcredentials.h"
#include "gui/systray.h"
#include "gui/tray/usermodel.h"
#include "gui/wizard/browserreauthcontroller.h"
#include "syncenginetestutils.h"
#include "theme.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QEvent>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

#include <algorithm>
#include <memory>

using namespace OCC;

class BrowserOpeningHandler : public QObject
{
    Q_OBJECT

public Q_SLOTS:
    void openUrl(const QUrl &url)
    {
        emit urlOpened(url);
    }

Q_SIGNALS:
    void urlOpened(const QUrl &url);
};

namespace {

class TestWebFlowCredentials final : public WebFlowCredentials
{
public:
    explicit TestWebFlowCredentials(QNetworkAccessManager *networkAccessManager)
        : WebFlowCredentials(QStringLiteral("old-user"), QStringLiteral("old-password"))
        , _networkAccessManager(networkAccessManager)
    {
    }

    [[nodiscard]] QNetworkAccessManager *createQNAM() const override
    {
        return _networkAccessManager;
    }

    void persist() override
    {
        ++_persistCount;
        _persistedUser = user();
        _persistedPassword = password();
    }

    [[nodiscard]] int persistCount() const
    {
        return _persistCount;
    }

    [[nodiscard]] QString persistedUser() const
    {
        return _persistedUser;
    }

    [[nodiscard]] QString persistedPassword() const
    {
        return _persistedPassword;
    }

private:
    QNetworkAccessManager *_networkAccessManager = nullptr;
    int _persistCount = 0;
    QString _persistedUser;
    QString _persistedPassword;
};

QQuickWindow *findBrowserReAuthWindow()
{
    const auto windows = QGuiApplication::topLevelWindows();
    const auto window = std::find_if(windows.cbegin(), windows.cend(), [](QWindow *window) {
        return window->property("controller").value<QObject *>() != nullptr;
    });
    return window == windows.cend() ? nullptr : qobject_cast<QQuickWindow *>(*window);
}

void configureSuccessfulLoginFlow(FakeQNAM *networkAccessManager,
                                  const QString &loginName = QStringLiteral("alice"),
                                  const QString &davUserId = QStringLiteral("alice"))
{
    networkAccessManager->setOverride([networkAccessManager, loginName, davUserId](
                                          QNetworkAccessManager::Operation operation,
                                          const QNetworkRequest &request,
                                          QIODevice *) -> QNetworkReply * {
        if (request.url().path() == QStringLiteral("/index.php/login/v2")) {
            const auto response = QJsonObject {
                {QStringLiteral("poll"), QJsonObject {
                                             {QStringLiteral("token"), QStringLiteral("poll-token")},
                                             {QStringLiteral("endpoint"), QStringLiteral("http://cloud.example/login/v2/poll")},
                                         }},
                {QStringLiteral("login"), QStringLiteral("http://cloud.example/login/v2/flow")},
            };
            return new FakeJsonReply(operation, request, networkAccessManager, 200, QJsonDocument(response));
        }

        if (request.url().path() == QStringLiteral("/login/v2/poll")) {
            const auto response = QJsonObject {
                {QStringLiteral("server"), QStringLiteral("http://cloud.example")},
                {QStringLiteral("loginName"), loginName},
                {QStringLiteral("appPassword"), QStringLiteral("app-password")},
            };
            return new FakeJsonReply(operation, request, networkAccessManager, 200, QJsonDocument(response));
        }

        // The re-authentication verifies the identity by fetching the canonical
        // dav_user (the persistent, unique account id) before persisting.
        if (request.url().path() == QStringLiteral("/ocs/v1.php/cloud/user")) {
            const auto response = QJsonObject {
                {QStringLiteral("ocs"), QJsonObject {
                                            {QStringLiteral("meta"), QJsonObject {
                                                                         {QStringLiteral("status"), QStringLiteral("ok")},
                                                                         {QStringLiteral("statuscode"), 100},
                                                                     }},
                                            {QStringLiteral("data"), QJsonObject {
                                                                         {QStringLiteral("id"), davUserId},
                                                                         {QStringLiteral("display-name"), davUserId},
                                                                     }},
                                        }},
            };
            return new FakeJsonReply(operation, request, networkAccessManager, 200, QJsonDocument(response));
        }

        return nullptr;
    });
}

} // namespace

class TestBrowserReAuthController : public QObject
{
    Q_OBJECT

    std::unique_ptr<FolderMan> _fm;

private slots:
    void initTestCase()
    {
        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);

        QStandardPaths::setTestModeEnabled(true);

        Q_INIT_RESOURCE(resources);
        Q_INIT_RESOURCE(theme);
        qmlRegisterSingletonInstance("com.nextcloud.desktopclient", 1, 0, "UserModel", UserModel::instance());
        qmlRegisterSingletonInstance("com.nextcloud.desktopclient", 1, 0, "Theme", Theme::instance());

        _fm.reset(new FolderMan{});

        Systray::instance()->setTrayEngine(new QQmlApplicationEngine(QCoreApplication::instance()));
    }

    void cleanup()
    {
        QDesktopServices::unsetUrlHandler(QStringLiteral("http"));

        if (auto *window = findBrowserReAuthWindow()) {
            window->close();
        }
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

    void sharedWizardWindowDoesNotEnableMinimizationByDefault()
    {
        QQmlComponent component(
            Systray::instance()->trayEngine(),
            QUrl(QStringLiteral("qrc:/qml/src/gui/WizardStyledWindow.qml")));
        const auto window = std::unique_ptr<QObject>(component.create());

        QVERIFY2(window, qPrintable(component.errorString()));
        const auto quickWindow = qobject_cast<QQuickWindow *>(window.get());
        QVERIFY(quickWindow);
        QVERIFY(!quickWindow->flags().testFlag(Qt::WindowMinimizeButtonHint));
    }

    void replacesAndPersistsCredentialsThroughQmlWindow()
    {
        BrowserOpeningHandler browserOpeningHandler;
        QDesktopServices::setUrlHandler(
            QStringLiteral("http"),
            &browserOpeningHandler,
            "openUrl");
        QSignalSpy browserSpy(&browserOpeningHandler, &BrowserOpeningHandler::urlOpened);

        const auto account = AccountManager::createAccount();
        account->setUrl(QUrl(QStringLiteral("http://cloud.example")));
        auto *const networkAccessManager = new FakeQNAM({});
        configureSuccessfulLoginFlow(networkAccessManager);
        auto *const credentials = new TestWebFlowCredentials(networkAccessManager);
        account->setCredentials(credentials);

        const auto registeredAccountCount = AccountManager::instance()->accounts().size();
        QSignalSpy askedSpy(credentials, &AbstractCredentials::asked);

        credentials->askFromUser();

        QQuickWindow *window = nullptr;
        QTRY_VERIFY_WITH_TIMEOUT((window = findBrowserReAuthWindow()) != nullptr, 5000);
        QVERIFY(window->flags().testFlag(Qt::WindowMinimizeButtonHint));
        QPointer<QQuickWindow> windowGuard(window);
        QTRY_COMPARE_WITH_TIMEOUT(browserSpy.count(), 1, 5000);

        auto *const controller = window->property("controller").value<QObject *>();
        QVERIFY(controller);
        QVERIFY(QMetaObject::invokeMethod(controller, "pollNow"));

        QTRY_COMPARE_WITH_TIMEOUT(askedSpy.count(), 1, 5000);
        QCOMPARE(credentials->user(), QStringLiteral("alice"));
        QCOMPARE(credentials->password(), QStringLiteral("app-password"));
        QCOMPARE(credentials->persistCount(), 1);
        QCOMPARE(credentials->persistedUser(), QStringLiteral("alice"));
        QCOMPARE(credentials->persistedPassword(), QStringLiteral("app-password"));
        // The account had no dav_user yet, so the fetched id is adopted as the
        // persistent identity.
        QCOMPARE(account->davUser(), QStringLiteral("alice"));
        QCOMPARE(AccountManager::instance()->accounts().size(), registeredAccountCount);
        QTRY_VERIFY_WITH_TIMEOUT(windowGuard.isNull(), 5000);
    }

    void updatesWebflowUserWhileKeepingDavUserConstant()
    {
        BrowserOpeningHandler browserOpeningHandler;
        QDesktopServices::setUrlHandler(
            QStringLiteral("http"),
            &browserOpeningHandler,
            "openUrl");
        QSignalSpy browserSpy(&browserOpeningHandler, &BrowserOpeningHandler::urlOpened);

        const auto account = AccountManager::createAccount();
        account->setUrl(QUrl(QStringLiteral("http://cloud.example")));
        // The persistent, unique account id.
        account->setDavUser(QStringLiteral("alice"));

        auto *const networkAccessManager = new FakeQNAM({});
        // The browser returns a different login name, but the server still maps
        // it to the same canonical dav_user "alice".
        configureSuccessfulLoginFlow(networkAccessManager, QStringLiteral("alice-renamed"), QStringLiteral("alice"));
        auto *const credentials = new TestWebFlowCredentials(networkAccessManager);
        account->setCredentials(credentials);

        QSignalSpy askedSpy(credentials, &AbstractCredentials::asked);

        credentials->askFromUser();

        QQuickWindow *window = nullptr;
        QTRY_VERIFY_WITH_TIMEOUT((window = findBrowserReAuthWindow()) != nullptr, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(browserSpy.count(), 1, 5000);

        auto *const controller = window->property("controller").value<QObject *>();
        QVERIFY(controller);
        QVERIFY(QMetaObject::invokeMethod(controller, "pollNow"));

        QTRY_COMPARE_WITH_TIMEOUT(askedSpy.count(), 1, 5000);
        // webflow_user (login name) was updated to the new value...
        QCOMPARE(credentials->user(), QStringLiteral("alice-renamed"));
        QCOMPARE(credentials->persistCount(), 1);
        QCOMPARE(credentials->persistedUser(), QStringLiteral("alice-renamed"));
        // ...while the persistent dav_user stayed constant.
        QCOMPARE(account->davUser(), QStringLiteral("alice"));
    }

    void rejectsReAuthenticationWithDifferentDavUser()
    {
        BrowserOpeningHandler browserOpeningHandler;
        QDesktopServices::setUrlHandler(
            QStringLiteral("http"),
            &browserOpeningHandler,
            "openUrl");
        QSignalSpy browserSpy(&browserOpeningHandler, &BrowserOpeningHandler::urlOpened);

        const auto account = AccountManager::createAccount();
        account->setUrl(QUrl(QStringLiteral("http://cloud.example")));
        account->setDavUser(QStringLiteral("alice"));

        auto *const networkAccessManager = new FakeQNAM({});
        // The user authenticated as a different account (canonical dav_user "bob").
        configureSuccessfulLoginFlow(networkAccessManager, QStringLiteral("bob"), QStringLiteral("bob"));
        auto *const credentials = new TestWebFlowCredentials(networkAccessManager);
        account->setCredentials(credentials);

        QSignalSpy askedSpy(credentials, &AbstractCredentials::asked);

        credentials->askFromUser();

        QQuickWindow *window = nullptr;
        QTRY_VERIFY_WITH_TIMEOUT((window = findBrowserReAuthWindow()) != nullptr, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(browserSpy.count(), 1, 5000);

        auto *const controller = window->property("controller").value<QObject *>();
        QVERIFY(controller);
        QVERIFY(QMetaObject::invokeMethod(controller, "pollNow"));

        // dav_user must stay constant: the mismatching login is rejected and the
        // re-authentication browser window is reopened.
        QTRY_COMPARE_WITH_TIMEOUT(browserSpy.count(), 2, 5000);
        QCOMPARE(askedSpy.count(), 0);
        QCOMPARE(credentials->persistCount(), 0);
        QCOMPARE(account->davUser(), QStringLiteral("alice"));
        QCOMPARE(credentials->user(), QStringLiteral("old-user"));

        // The rejection reopens a re-auth window; close every one and wait until
        // it is destroyed so it does not leak into the next test (cleanup() only
        // closes a single window and does not wait for its destruction).
        const auto topLevelWindows = QGuiApplication::topLevelWindows();
        for (auto *const topLevelWindow : topLevelWindows) {
            if (topLevelWindow->property("controller").value<QObject *>() != nullptr) {
                topLevelWindow->close();
            }
        }
        QTRY_VERIFY_WITH_TIMEOUT(findBrowserReAuthWindow() == nullptr, 5000);
    }

    void closingQmlWindowCancelsWithoutReplacingCredentials()
    {
        const auto account = AccountManager::createAccount();
        account->setUrl(QUrl(QStringLiteral("http://cloud.example")));
        auto *const networkAccessManager = new FakeQNAM({});
        networkAccessManager->setOverride([networkAccessManager](
                                              QNetworkAccessManager::Operation operation,
                                              const QNetworkRequest &request,
                                              QIODevice *) -> QNetworkReply * {
            return new FakeHangingReply(operation, request, networkAccessManager);
        });
        auto *const credentials = new TestWebFlowCredentials(networkAccessManager);
        account->setCredentials(credentials);
        QSignalSpy askedSpy(credentials, &AbstractCredentials::asked);

        credentials->askFromUser();

        QQuickWindow *window = nullptr;
        QTRY_VERIFY_WITH_TIMEOUT((window = findBrowserReAuthWindow()) != nullptr, 5000);
        QPointer<QQuickWindow> windowGuard(window);

        QVERIFY(window->close());

        QTRY_COMPARE_WITH_TIMEOUT(askedSpy.count(), 1, 5000);
        QCOMPARE(credentials->user(), QStringLiteral("old-user"));
        QCOMPARE(credentials->password(), QStringLiteral("old-password"));
        QCOMPARE(credentials->persistCount(), 0);
        QTRY_VERIFY_WITH_TIMEOUT(windowGuard.isNull(), 5000);
    }

    void exposesAuthenticationErrorsToQml()
    {
        const auto account = AccountManager::createAccount();
        BrowserReAuthController controller(account.data());
        QSignalSpy errorSpy(&controller, &BrowserReAuthController::errorTextChanged);

        controller.slotAuthResult(Flow2Auth::Error, QStringLiteral("Authentication failed."), {}, {});

        QVERIFY(!controller.finished());
        QCOMPARE(controller.errorText(), QStringLiteral("Authentication failed."));
        QCOMPARE(errorSpy.count(), 1);
    }

    void keepsPollingWhenOpeningTheBrowserFails()
    {
        const auto account = AccountManager::createAccount();
        BrowserReAuthController controller(account.data());

        controller.slotStatusChanged(Flow2Auth::statusPollCountdown, 3);
        controller.slotAuthResult(Flow2Auth::NotSupported, {}, {}, {});

        QVERIFY(controller.authPolling());
        QVERIFY(!controller.busy());
        QVERIFY(!controller.finished());
        QVERIFY(!controller.errorText().isEmpty());
    }

    void cancellationFinishesWithoutCredentials()
    {
        const auto account = AccountManager::createAccount();
        BrowserReAuthController controller(account.data());
        QSignalSpy credentialsSpy(&controller, &BrowserReAuthController::credentialsReady);
        QSignalSpy cancelledSpy(&controller, &BrowserReAuthController::cancelled);

        controller.cancel();

        QVERIFY(controller.finished());
        QCOMPARE(credentialsSpy.count(), 0);
        QCOMPARE(cancelledSpy.count(), 1);
    }
};

QTEST_MAIN(TestBrowserReAuthController)
#include "testbrowserreauthcontroller.moc"
