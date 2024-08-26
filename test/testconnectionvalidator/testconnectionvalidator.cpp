/*
 * Copyright (C) by Hannah von Reth <hannah.vonreth@owncloud.com>
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

#include <QtTest>

#include "gui/connectionvalidator.h"
#include "libsync/abstractnetworkjob.h"
#include "libsync/httplogger.h"
#include "resources/template.h"

#include "testutils/syncenginetestutils.h"
#include "testutils/testutils.h"

using namespace std::chrono_literals;

using namespace OCC;

class TestConnectionValidator : public QObject
{
    Q_OBJECT

    enum class FailStage { Invalid, StatusPhp, AuthValidation, Capabilities, UserInfo };

    // we can't use QMap direclty with QFETCH
    using Values = QMap<QString, QString>;
    auto getPayload(const QString &payloadName)
    {
        QFile f(QStringLiteral(SOURCEDIR "/test/testconnectionvalidator/%1").arg(payloadName));
        Q_ASSERT(f.open(QIODevice::ReadOnly));
        return f.readAll();
    }

    auto getPayloadTemplated(const QString &payloadName, const QMap<QString, QString> &values)
    {
        return Resources::Template::renderTemplate(QString::fromUtf8(getPayload(payloadName)), values).toUtf8();
    }

private Q_SLOTS:


    void initTestCase() { AbstractNetworkJob::httpTimeout = 1s; }

    void testStatusPhp_data()
    {
        QTest::addColumn<FailStage>("failStage");
        QTest::addColumn<Values>("values");
        QTest::addColumn<ConnectionValidator::Status>("status");

        const auto defaultValue = Values{{QStringLiteral("maintenance"), QStringLiteral("false")}, {QStringLiteral("version"), QStringLiteral("10.11.0.0")},
            {QStringLiteral("productversion"), QStringLiteral("4.0.5")}};

        QTest::newRow("stauts.php maintenance") << FailStage::StatusPhp << [value = defaultValue]() mutable {
            value[QStringLiteral("maintenance")] = QStringLiteral("true");
            return value;
        }() << ConnectionValidator::MaintenanceMode;
        QTest::newRow("stauts.php ServiceUnavailable") << FailStage::StatusPhp << defaultValue << ConnectionValidator::StatusNotFound;
        QTest::newRow("stauts.php UnsupportedClient") << FailStage::StatusPhp << defaultValue << ConnectionValidator::ClientUnsupported;

        QTest::newRow("auth 401") << FailStage::AuthValidation << defaultValue << ConnectionValidator::CredentialsWrong;
        QTest::newRow("auth timeout") << FailStage::AuthValidation << defaultValue << ConnectionValidator::Timeout;
        QTest::newRow("auth ServiceUnavailable") << FailStage::AuthValidation << defaultValue << ConnectionValidator::ServiceUnavailable;
        QTest::newRow("auth UnsupportedClient") << FailStage::AuthValidation << defaultValue << ConnectionValidator::ClientUnsupported;

        QTest::newRow("capabilites timeout") << FailStage::Capabilities << defaultValue << ConnectionValidator::CredentialsWrong;
        QTest::newRow("capabilites 401") << FailStage::Capabilities << defaultValue << ConnectionValidator::Timeout;
        QTest::newRow("capabilites unsupported server") << FailStage::Capabilities << [value = defaultValue]() mutable {
            value[QStringLiteral("version")] = QStringLiteral("7.0");
            value[QStringLiteral("productversion")] = QString();
            return value;
        }() << ConnectionValidator::ServerVersionMismatch;


        QTest::newRow("user info timeout") << FailStage::UserInfo << defaultValue << ConnectionValidator::Timeout;
        QTest::newRow("user info 401") << FailStage::UserInfo << defaultValue << ConnectionValidator::CredentialsWrong;
        QTest::newRow("success") << FailStage::UserInfo << defaultValue << ConnectionValidator::Connected;
    }

    void testStatusPhp()
    {
        QFETCH(FailStage, failStage);
        QFETCH(Values, values);
        QFETCH(ConnectionValidator::Status, status);

        auto reachedStage = FailStage::Invalid;
        FakeFolder fakeFolder({});

        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *) -> QNetworkReply * {
            const auto path = request.url().path();
            const auto verb = HttpLogger::requestVerb(op, request);
            if (op == QNetworkAccessManager::GetOperation) {
                if (path.endsWith(QLatin1String("status.php"))) {
                    reachedStage = FailStage::StatusPhp;
                    if (failStage == FailStage::StatusPhp) {
                        if (status == ConnectionValidator::Timeout) {
                            return new FakeHangingReply(op, request, this);
                        } else if (status == ConnectionValidator::StatusNotFound) {
                            return new FakeErrorReply(op, request, this, 500);
                        } else if (status == ConnectionValidator::ClientUnsupported) {
                            return new FakeErrorReply(op, request, this, 403);
                        }
                    }
                    return new FakePayloadReply(op, request, getPayloadTemplated(QStringLiteral("status.php.json.in"), values), this);
                } else if (path.endsWith(QLatin1String("capabilities"))) {
                    reachedStage = FailStage::Capabilities;
                    if (failStage == FailStage::Capabilities) {
                        if (status == ConnectionValidator::CredentialsWrong) {
                            return new FakeErrorReply(op, request, this, 401);
                        } else if (status == ConnectionValidator::Timeout) {
                            return new FakeHangingReply(op, request, this);
                        }
                    }
                    return new FakePayloadReply(op, request, getPayloadTemplated(QStringLiteral("capabilities.json.in"), values), this);
                } else if (path.endsWith(QLatin1String("user"))) {
                    reachedStage = FailStage::UserInfo;
                    if (failStage == FailStage::UserInfo) {
                        if (status == ConnectionValidator::CredentialsWrong) {
                            return new FakeErrorReply(op, request, this, 401);
                        } else if (status == ConnectionValidator::Timeout) {
                            return new FakeHangingReply(op, request, this);
                        }
                    }
                    return new FakePayloadReply(op, request, getPayload(QStringLiteral("user.json")), this);
                }
            } else if (failStage == FailStage::AuthValidation && verb == "PROPFIND") {
                reachedStage = FailStage::AuthValidation;
                if (status == ConnectionValidator::CredentialsWrong) {
                    return new FakeErrorReply(op, request, this, 401);
                } else if (status == ConnectionValidator::Timeout) {
                    return new FakeHangingReply(op, request, this);
                } else if (status == ConnectionValidator::ServiceUnavailable) {
                    return new FakeErrorReply(op, request, this, 503);
                } else if (status == ConnectionValidator::ClientUnsupported) {
                    return new FakeErrorReply(op, request, this, 403);
                }
            }
            return nullptr;
        });

        ConnectionValidator val(fakeFolder.account());
        val.checkServer(ConnectionValidator::ValidationMode::ValidateAuthAndUpdate);

        QSignalSpy spy(&val, &ConnectionValidator::connectionResult);
        QVERIFY(spy.wait());
        QCOMPARE(spy.first().first().value<ConnectionValidator::Status>(), status);
        QCOMPARE(reachedStage, failStage);
    }
};

QTEST_MAIN(TestConnectionValidator)
#include "testconnectionvalidator.moc"
