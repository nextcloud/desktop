/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "account.h"
#include "gui/connectionvalidator.h"
#include "gui/localnetworkpermission.h"
#include "testhelper.h"

#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

using namespace OCC;

namespace OCC {

class ConnectionValidatorTestAccess
{
public:
    static void setLocalNetworkPermissionDenied(ConnectionValidator &validator, bool denied)
    {
        validator._localNetworkPermissionCheck = [denied](const QUrl &, QObject *, std::function<void(bool)> callback) {
            callback(denied);
        };
    }

    static void reportTimeout(ConnectionValidator &validator, const QUrl &url)
    {
        validator.slotJobTimeout(url);
    }
};

}

class TestConnectionValidator : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void localNetworkPermissionFailureReplacesTimeout()
    {
        const auto account = Account::create();
        account->setUrl(QUrl(QStringLiteral("https://cloud.example")));
        const auto accountState = AccountStatePtr(new FakeAccountState(account));
        ConnectionValidator validator(accountState, {});
        ConnectionValidatorTestAccess::setLocalNetworkPermissionDenied(validator, true);
        QSignalSpy resultSpy(&validator, &ConnectionValidator::connectionResult);

        ConnectionValidatorTestAccess::reportTimeout(validator, account->url());

        QCOMPARE(resultSpy.count(), 1);
        const auto result = resultSpy.takeFirst();
        QCOMPARE(result.at(0).value<ConnectionValidator::Status>(), ConnectionValidator::Timeout);
        QCOMPARE(result.at(1).toStringList(), QStringList({LocalNetworkPermission::deniedError()}));
    }
};

QTEST_MAIN(TestConnectionValidator)
#include "testconnectionvalidator.moc"
