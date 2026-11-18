/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "account.h"
#include "gui/connectionvalidator.h"
#include "gui/localnetworkpermission.h"
#include "syncenginetestutils.h"
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

    static void callSlotCapabilitiesEtagReceived(ConnectionValidator &validator, const QByteArray &value, int statusCode)
    {
        validator.slotCapabilitiesEtagReceived(value, statusCode);
    }

    static void callSlotCapabilitiesReceived(ConnectionValidator &validator, const QJsonDocument &json, int statusCode)
    {
        validator.slotCapabilitiesReceived(json, statusCode);
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

    void capabilitiesEtagReceivedAndApplied()
    {
        const auto account = Account::create();
        account->setUrl(QUrl(QStringLiteral("https://cloud.example")));
        account->setCredentials(new FakeCredentials{new FakeQNAM({})});
        const auto accountState = AccountStatePtr(new FakeAccountState(account));
        ConnectionValidator validator(accountState, {});

        const auto json = QJsonDocument::fromJson(R"({
            "ocs": {
                "meta": {"status": "ok", "statuscode": 100},
                "data": {
                    "capabilities": {
                        "core": {"status": {"version": "28.0.0"}},
                        "files_sharing": {"api_enabled": true}
                    }
                }
            }
        })");

        const auto etag = QByteArrayLiteral("\"etag-123\"");
        ConnectionValidatorTestAccess::callSlotCapabilitiesEtagReceived(validator, etag, 100);
        ConnectionValidatorTestAccess::callSlotCapabilitiesReceived(validator, json, 100);

        QCOMPARE(account->capabilitiesEtag(), etag);
        QVERIFY(account->capabilities().isValid());
        QVERIFY(account->capabilities().shareAPI());
        QCOMPARE(account->serverVersion(), QStringLiteral("28.0.0"));
    }

    void capabilitiesNotModifiedPreservesExistingCapabilities()
    {
        const auto account = Account::create();
        account->setUrl(QUrl(QStringLiteral("https://cloud.example")));
        account->setCredentials(new FakeCredentials{new FakeQNAM({})});
        const auto accountState = AccountStatePtr(new FakeAccountState(account));
        ConnectionValidator validator(accountState, {});

        const auto json = QJsonDocument::fromJson(R"({
            "ocs": {
                "meta": {"status": "ok", "statuscode": 100},
                "data": {
                    "capabilities": {
                        "core": {"status": {"version": "28.0.0"}},
                        "files_sharing": {"api_enabled": true}
                    }
                }
            }
        })");

        const auto etag = QByteArrayLiteral("\"etag-123\"");
        ConnectionValidatorTestAccess::callSlotCapabilitiesEtagReceived(validator, etag, 100);
        ConnectionValidatorTestAccess::callSlotCapabilitiesReceived(validator, json, 100);

        // Now simulate a 304 Not Modified response with empty JSON
        ConnectionValidatorTestAccess::callSlotCapabilitiesEtagReceived(validator, etag, 304);
        ConnectionValidatorTestAccess::callSlotCapabilitiesReceived(validator, QJsonDocument(), 304);

        // Existing capabilities must be preserved
        QCOMPARE(account->capabilitiesEtag(), etag);
        QVERIFY(account->capabilities().isValid());
        QVERIFY(account->capabilities().shareAPI());
        QCOMPARE(account->serverVersion(), QStringLiteral("28.0.0"));
    }

    void capabilitiesUpdatedWithNewEtag()
    {
        const auto account = Account::create();
        account->setUrl(QUrl(QStringLiteral("https://cloud.example")));
        account->setCredentials(new FakeCredentials{new FakeQNAM({})});
        const auto accountState = AccountStatePtr(new FakeAccountState(account));
        ConnectionValidator validator(accountState, {});

        const auto initialJson = QJsonDocument::fromJson(R"({
            "ocs": {
                "meta": {"status": "ok", "statuscode": 100},
                "data": {
                    "capabilities": {
                        "core": {"status": {"version": "28.0.0"}},
                        "files_sharing": {"api_enabled": true}
                    }
                }
            }
        })");
        const auto initialEtag = QByteArrayLiteral("\"etag-initial\"");
        ConnectionValidatorTestAccess::callSlotCapabilitiesEtagReceived(validator, initialEtag, 100);
        ConnectionValidatorTestAccess::callSlotCapabilitiesReceived(validator, initialJson, 100);

        const auto updatedJson = QJsonDocument::fromJson(R"({
            "ocs": {
                "meta": {"status": "ok", "statuscode": 100},
                "data": {
                    "capabilities": {
                        "core": {"status": {"version": "29.0.0"}},
                        "files_sharing": {"api_enabled": false}
                    }
                }
            }
        })");
        const auto updatedEtag = QByteArrayLiteral("\"etag-updated\"");
        ConnectionValidatorTestAccess::callSlotCapabilitiesEtagReceived(validator, updatedEtag, 100);
        ConnectionValidatorTestAccess::callSlotCapabilitiesReceived(validator, updatedJson, 100);

        QCOMPARE(account->capabilitiesEtag(), updatedEtag);
        QVERIFY(account->capabilities().isValid());
        QCOMPARE(account->capabilities().shareAPI(), false);
        QCOMPARE(account->serverVersion(), QStringLiteral("29.0.0"));
    }
};

QTEST_MAIN(TestConnectionValidator)
#include "testconnectionvalidator.moc"
