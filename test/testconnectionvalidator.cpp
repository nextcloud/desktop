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

class LocalNetworkPermissionConnectionValidator : public ConnectionValidator
{
public:
    using ConnectionValidator::ConnectionValidator;

    bool localNetworkPermissionDenied = false;

    void reportTimeout(const QUrl &url)
    {
        slotJobTimeout(url);
    }

protected:
    void checkLocalNetworkPermissionDenied(const QUrl &, std::function<void(bool)> callback) override
    {
        callback(localNetworkPermissionDenied);
    }
};

class TestConnectionValidator : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void localNetworkPermissionFailureReplacesTimeout()
    {
        const auto account = Account::create();
        account->setUrl(QUrl(QStringLiteral("https://cloud.example")));
        const auto accountState = AccountStatePtr(new FakeAccountState(account));
        LocalNetworkPermissionConnectionValidator validator(accountState, {});
        validator.localNetworkPermissionDenied = true;
        QSignalSpy resultSpy(&validator, &ConnectionValidator::connectionResult);

        validator.reportTimeout(account->url());

        QCOMPARE(resultSpy.count(), 1);
        const auto result = resultSpy.takeFirst();
        QCOMPARE(result.at(0).value<ConnectionValidator::Status>(), ConnectionValidator::Timeout);
        QCOMPARE(result.at(1).toStringList(), QStringList({LocalNetworkPermission::deniedError()}));
    }
};

QTEST_MAIN(TestConnectionValidator)
#include "testconnectionvalidator.moc"
