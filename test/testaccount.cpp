/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2017 ownCloud GmbH
 * SPDX-License-Identifier: CC0-1.0
 *
 * This software is in the public domain, furnished "as is", without technical
 * support, and with no warranty, express or implied, as to its usefulness for
 * any purpose.
 */

#include <qglobal.h>
#include <QTemporaryDir>
#include <QtTest>

#include "account.h"
#include "accountstate.h"
#include "logger.h"
#include "syncenginetestutils.h"

using namespace OCC;

class TestAccount: public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);

        QStandardPaths::setTestModeEnabled(true);
    }

    void testAccountDavPath_unitialized_noCrash()
    {
        AccountPtr account = Account::create();
        [[maybe_unused]] const auto davPath = account->davPath();
    }

    void testAccount_isPublicShareLink_data()
    {
        QTest::addColumn<QString>("url");
        QTest::addColumn<bool>("expectedResult");
        QTest::addColumn<QString>("expectedDavUser");

        QTest::newRow("plain URL") << "https://example.com" << false << "";
        QTest::newRow("plain URL, trailing slash") << "https://example.com/" << false << "";
        QTest::newRow("share link") << "https://example.com/s/rPZLaTKfWct37Nd" << true << "rPZLaTKfWct37Nd";
        QTest::newRow("share link, trailing slash") << "https://example.com/s/rPZLaTKfWct37Nd/" << false << "";
        QTest::newRow("subpath containing /s/ (looks like share link)") << "https://example.com/s/nextcloud" << true << "nextcloud";
        QTest::newRow("subpath containing /s/, trailing slash") << "https://example.com/s/nextcloud/" << false << "";
        QTest::newRow("subpath containing /s/, share link") << "https://example.com/s/nextcloud/s/rPZLaTKfWct37Nd" << true << "rPZLaTKfWct37Nd";
        QTest::newRow("subpath containing /s/, share link, trailing slash") << "https://example.com/s/nextcloud/s/rPZLaTKfWct37Nd/" << false << "";
    }

    void testAccount_isPublicShareLink()
    {
        AccountPtr account = Account::create();

        QFETCH(QString, url);
        QFETCH(bool, expectedResult);
        QFETCH(QString, expectedDavUser);

        account->setUrl(QUrl{url});
        QCOMPARE(account->isPublicShareLink(), expectedResult);
        QCOMPARE(account->davUser(), expectedDavUser);
    }

    void testAccount_setLimitSettings_globalNetworkLimitFallback()
    {
        using LimitSetting = Account::AccountNetworkTransferLimitSetting;
        AccountPtr account = Account::create();

        const auto setLimitSettings = [account](const LimitSetting setting) -> void {
            account->setDownloadLimitSetting(setting);
            account->setUploadLimitSetting(setting);
        };

        const auto verifyLimitSettings = [account](const LimitSetting expectedSetting) -> void {
            QCOMPARE_EQ(expectedSetting, account->downloadLimitSetting());
            QCOMPARE_EQ(expectedSetting, account->uploadLimitSetting());
        };

        // the default setting should be NoLimit
        verifyLimitSettings(LimitSetting::NoLimit);

        // changing it to ManualLimit should succeed
        setLimitSettings(LimitSetting::ManualLimit);
        verifyLimitSettings(LimitSetting::ManualLimit);

        // changing it to AutoLimit should succeed
        setLimitSettings(LimitSetting::AutoLimit);
        verifyLimitSettings(LimitSetting::AutoLimit);

        // changing it to LegacyGlobalLimit (-2) should fall back to NoLimit
        setLimitSettings(LimitSetting::LegacyGlobalLimit);
        verifyLimitSettings(LimitSetting::NoLimit);
    }

    void testAccount_listRemoteFolder_data()
    {
        QTest::addColumn<QString>("remotePath");
        QTest::addColumn<QString>("subPath");
        QTest::addColumn<QStringList>("expectedPaths");

        QTest::newRow("root = /; requesting ''") << "" << "" << QStringList{ "A", "B", "C", "S" };
        QTest::newRow("root = /; requesting A/") << "" << "A/" << QStringList{ "A/a1", "A/a2", "A/sub1" };
        QTest::newRow("root = /; requesting A/sub1/") << "" << "A/sub1/" << QStringList{ "A/sub1/sub2" };
        QTest::newRow("root = /; requesting A/sub1/sub2") << "" << "A/sub1/sub2/" << QStringList{};
        QTest::newRow("root = /A; requesting A/") << "/A" << "A/" << QStringList{ "a1", "a2", "sub1" };
        QTest::newRow("root = /A; requesting A/sub1") << "/A" << "A/sub1/" << QStringList{ "sub1/sub2" };
        QTest::newRow("root = /A; requesting A/sub1/sub2") << "/A" << "A/sub1/sub2/" << QStringList{};
    }

    void testAccount_listRemoteFolder()
    {
        QFETCH(QString, remotePath);
        QFETCH(QString, subPath);
        QFETCH(QStringList, expectedPaths);

        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        fakeFolder.remoteModifier().mkdir("A/sub1");
        fakeFolder.remoteModifier().mkdir("A/sub1/sub2");
        const auto account = fakeFolder.account();

        QEventLoop localEventLoop;
        auto lsPropPromise = QPromise<OCC::PlaceholderCreateInfo>{};
        auto lsPropFuture = lsPropPromise.future();
        auto lsPropFutureWatcher = QFutureWatcher<OCC::PlaceholderCreateInfo>{};
        lsPropFutureWatcher.setFuture(lsPropFuture);

        QList<QString> receivedPaths;

        QObject::connect(&lsPropFutureWatcher, &QFutureWatcher<QStringList>::finished,
                            &localEventLoop, [&localEventLoop] () {
                                qInfo() << "ls prop finished";
                                localEventLoop.quit();
                            });

        QObject::connect(&lsPropFutureWatcher, &QFutureWatcher<QStringList>::resultReadyAt,
                            &localEventLoop, [&receivedPaths, &lsPropFutureWatcher] (int resultIndex) {
                                qInfo() << "ls prop result at index" << resultIndex;
                                const auto &newResultEntries = lsPropFutureWatcher.resultAt(resultIndex);
                                receivedPaths.append(newResultEntries.fullPath);
                            });

        fakeFolder.syncJournal().clearFileTable(); // pretend we only need to fetch placeholders

        account->listRemoteFolder(&lsPropPromise, remotePath, subPath, &fakeFolder.syncJournal());
        localEventLoop.exec();

        receivedPaths.sort();
        expectedPaths.sort();
        QCOMPARE_EQ(receivedPaths, expectedPaths);
    }
};

QTEST_GUILESS_MAIN(TestAccount)
#include "testaccount.moc"
