/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "gui/accountmanager.h"
#include "gui/wizard/browserreauthcontroller.h"

#include <QSignalSpy>
#include <QTest>

using namespace OCC;

class TestBrowserReAuthController : public QObject
{
    Q_OBJECT

private slots:
    void returnsReplacementCredentialsWithoutStartingAccountSetup()
    {
        const auto account = AccountManager::createAccount();
        const auto registeredAccountCount = AccountManager::instance()->accounts().size();
        BrowserReAuthController controller(account.data());
        QSignalSpy credentialsSpy(&controller, &BrowserReAuthController::credentialsReady);
        QSignalSpy cancelledSpy(&controller, &BrowserReAuthController::cancelled);

        controller.slotAuthResult(Flow2Auth::LoggedIn, {}, QStringLiteral("alice"), QStringLiteral("app-password"));

        QVERIFY(controller.finished());
        QCOMPARE(credentialsSpy.count(), 1);
        QCOMPARE(credentialsSpy.first().at(0).toString(), QStringLiteral("alice"));
        QCOMPARE(credentialsSpy.first().at(1).toString(), QStringLiteral("app-password"));
        QCOMPARE(AccountManager::instance()->accounts().size(), registeredAccountCount);

        controller.cancel();
        QCOMPARE(cancelledSpy.count(), 0);
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
