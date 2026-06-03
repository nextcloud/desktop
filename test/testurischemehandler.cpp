/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: CC0-1.0
 */

#include <QtTest>

#include <QUrl>

#include "urischemehandler.h"

using namespace OCC;

Q_DECLARE_METATYPE(OCC::UriSchemeHandler::Action)

class TestUriSchemeHandler : public QObject
{
    Q_OBJECT

private slots:
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

        const auto result = UriSchemeHandler::parseUri(url);

        QCOMPARE(result.action, expectedAction);
        QCOMPARE(result.serverUrl, expectedServerUrl);
        QCOMPARE(result.error.isEmpty(), !expectError);
    }
};

QTEST_APPLESS_MAIN(TestUriSchemeHandler)
#include "testurischemehandler.moc"
