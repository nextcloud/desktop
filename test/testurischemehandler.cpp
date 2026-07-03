/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: CC0-1.0
 */

#include <QtTest>

#include <QScopeGuard>
#include <QStandardPaths>
#include <QUrl>

#include "configfile.h"
#include "theme.h"
#include "urischemehandler.h"

using namespace OCC;

Q_DECLARE_METATYPE(OCC::UriSchemeHandler::Action)

class TestUriSchemeHandler : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void parseUri_data()
    {
        QTest::addColumn<QUrl>("url");
        QTest::addColumn<UriSchemeHandler::Action>("expectedAction");
        QTest::addColumn<QUrl>("expectedServerUrl");
        QTest::addColumn<bool>("expectError");

        QTest::newRow("login")
            << QUrl(QStringLiteral("nc://login/server:https://cloud.example.com"))
            << UriSchemeHandler::Action::Login
            << QUrl(QStringLiteral("https://cloud.example.com"))
            << false;

        QTest::newRow("login encoded server")
            << QUrl(QStringLiteral("nc://login/server:https%3A%2F%2Fcloud.example.com"))
            << UriSchemeHandler::Action::Login
            << QUrl(QStringLiteral("https://cloud.example.com"))
            << false;

        QTest::newRow("login uppercase host")
            << QUrl(QStringLiteral("nc://LOGIN/server:https://cloud.example.com"))
            << UriSchemeHandler::Action::Login
            << QUrl(QStringLiteral("https://cloud.example.com"))
            << false;

        QTest::newRow("login missing server prefix")
            << QUrl(QStringLiteral("nc://login/https://cloud.example.com"))
            << UriSchemeHandler::Action::Invalid
            << QUrl{}
            << true;

        QTest::newRow("login missing server url")
            << QUrl(QStringLiteral("nc://login/server:"))
            << UriSchemeHandler::Action::Invalid
            << QUrl{}
            << true;

        QTest::newRow("login relative server url")
            << QUrl(QStringLiteral("nc://login/server:cloud.example.com"))
            << UriSchemeHandler::Action::Invalid
            << QUrl{}
            << true;

        QTest::newRow("login hostless server url")
            << QUrl(QStringLiteral("nc://login/server:https://"))
            << UriSchemeHandler::Action::Invalid
            << QUrl{}
            << true;

        QTest::newRow("login unsupported server url scheme")
            << QUrl(QStringLiteral("nc://login/server:ftp://cloud.example.com"))
            << UriSchemeHandler::Action::Invalid
            << QUrl{}
            << true;

        QTest::newRow("old add account")
            << QUrl(QStringLiteral("nc://addAccount/?serverUrl=https%3A%2F%2Fcloud.example.com"))
            << UriSchemeHandler::Action::Invalid
            << QUrl{}
            << true;

        QTest::newRow("local edit")
            << QUrl(QStringLiteral("nc://open/admin@cloud.example.com/Documents/file.txt?token=secret"))
            << UriSchemeHandler::Action::OpenLocalEdit
            << QUrl{}
            << false;

        QTest::newRow("unknown action")
            << QUrl(QStringLiteral("nc://unknown/"))
            << UriSchemeHandler::Action::Invalid
            << QUrl{}
            << true;
    }

    void parseUri()
    {
        QFETCH(QUrl, url);
        QFETCH(UriSchemeHandler::Action, expectedAction);
        QFETCH(QUrl, expectedServerUrl);
        QFETCH(bool, expectError);

        auto theme = Theme::instance();
        theme->setOverrideServerUrl(QString{});
        theme->setForceOverrideServerUrl(false);
        ConfigFile{}.setOverrideServerUrl(QString{});

        const auto result = UriSchemeHandler::parseUri(url);

        QCOMPARE(result.action, expectedAction);
        QCOMPARE(result.serverUrl, expectedServerUrl);
        QCOMPARE(result.error.isEmpty(), !expectError);
    }

    void parseUriHonoursForcedSingleServerOverride()
    {
        auto theme = Theme::instance();
        const auto previousOverrideServerUrl = theme->overrideServerUrl();
        const auto previousForceOverrideServerUrl = theme->forceOverrideServerUrl();
        const auto restoreTheme = qScopeGuard([theme, previousOverrideServerUrl, previousForceOverrideServerUrl] {
            theme->setOverrideServerUrl(previousOverrideServerUrl);
            theme->setForceOverrideServerUrl(previousForceOverrideServerUrl);
        });

        theme->setOverrideServerUrl(QStringLiteral("https://cloud.example.com"));
        theme->setForceOverrideServerUrl(true);

        const auto acceptedResult = UriSchemeHandler::parseUri(QUrl(QStringLiteral("nc://login/server:https://cloud.example.com/")));
        QCOMPARE(acceptedResult.action, UriSchemeHandler::Action::Login);
        QCOMPARE(acceptedResult.serverUrl, QUrl(QStringLiteral("https://cloud.example.com/")));
        QVERIFY(acceptedResult.error.isEmpty());

        const auto rejectedResult = UriSchemeHandler::parseUri(QUrl(QStringLiteral("nc://login/server:https://unconfigured.example.com")));
        QCOMPARE(rejectedResult.action, UriSchemeHandler::Action::Invalid);
        QVERIFY(!rejectedResult.error.isEmpty());
    }

    void parseUriHonoursConfiguredServerOverride()
    {
        ConfigFile cfg;
        const auto previousConfiguredOverrideServerUrl = cfg.overrideServerUrl();
        auto theme = Theme::instance();
        const auto previousOverrideServerUrl = theme->overrideServerUrl();
        const auto previousForceOverrideServerUrl = theme->forceOverrideServerUrl();
        const auto restoreState = qScopeGuard([&cfg, theme, previousConfiguredOverrideServerUrl, previousOverrideServerUrl, previousForceOverrideServerUrl] {
            cfg.setOverrideServerUrl(previousConfiguredOverrideServerUrl);
            theme->setOverrideServerUrl(previousOverrideServerUrl);
            theme->setForceOverrideServerUrl(previousForceOverrideServerUrl);
        });

        cfg.setOverrideServerUrl(QStringLiteral("https://configured.example.com"));
        theme->setOverrideServerUrl(QString{});
        theme->setForceOverrideServerUrl(false);

        const auto acceptedResult = UriSchemeHandler::parseUri(QUrl(QStringLiteral("nc://login/server:https://configured.example.com")));
        QCOMPARE(acceptedResult.action, UriSchemeHandler::Action::Login);
        QCOMPARE(acceptedResult.serverUrl, QUrl(QStringLiteral("https://configured.example.com")));
        QVERIFY(acceptedResult.error.isEmpty());

        const auto rejectedResult = UriSchemeHandler::parseUri(QUrl(QStringLiteral("nc://login/server:https://unconfigured.example.com")));
        QCOMPARE(rejectedResult.action, UriSchemeHandler::Action::Invalid);
        QVERIFY(!rejectedResult.error.isEmpty());
    }

    void parseUriHonoursMultipleServerOverrides()
    {
        auto theme = Theme::instance();
        const auto previousOverrideServerUrl = theme->overrideServerUrl();
        const auto previousForceOverrideServerUrl = theme->forceOverrideServerUrl();
        const auto restoreTheme = qScopeGuard([theme, previousOverrideServerUrl, previousForceOverrideServerUrl] {
            theme->setOverrideServerUrl(previousOverrideServerUrl);
            theme->setForceOverrideServerUrl(previousForceOverrideServerUrl);
        });

        theme->setOverrideServerUrl(QStringLiteral(
            "["
            R"({"name": "Primary", "url": "https://primary.example.com"},)"
            R"({"name": "Secondary", "url": "https://secondary.example.com"})"
            "]"));
        theme->setForceOverrideServerUrl(true);

        const auto acceptedResult = UriSchemeHandler::parseUri(QUrl(QStringLiteral("nc://login/server:https://secondary.example.com")));
        QCOMPARE(acceptedResult.action, UriSchemeHandler::Action::Login);
        QCOMPARE(acceptedResult.serverUrl, QUrl(QStringLiteral("https://secondary.example.com")));
        QVERIFY(acceptedResult.error.isEmpty());

        const auto rejectedResult = UriSchemeHandler::parseUri(QUrl(QStringLiteral("nc://login/server:https://unconfigured.example.com")));
        QCOMPARE(rejectedResult.action, UriSchemeHandler::Action::Invalid);
        QVERIFY(!rejectedResult.error.isEmpty());
    }
};

QTEST_APPLESS_MAIN(TestUriSchemeHandler)
#include "testurischemehandler.moc"
