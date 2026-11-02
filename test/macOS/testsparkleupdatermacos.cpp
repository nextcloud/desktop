/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "sparkleupdatertestcontroller.h"

#include "configfile.h"
#include "updater/sparkleupdater.h"

#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

using namespace Qt::StringLiterals;

class TestSparkleUpdaterMacOS : public QObject
{
    Q_OBJECT

    QTemporaryDir configDirectory;

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(configDirectory.isValid());
        QVERIFY(OCC::ConfigFile::setConfDir(configDirectory.path()));
    }

    void init()
    {
        SparkleUpdaterTestController::install();
    }

    void cleanup()
    {
        SparkleUpdaterTestController::restore();
    }

    void retainsDelegateControllerAndFeedUntilDestruction()
    {
        __weak id<SPUUpdaterDelegate> weakDelegate;
        __weak SPUStandardUpdaterController *weakController;
        {
            const auto updater = std::make_unique<OCC::SparkleUpdater>(QUrl("https://example.test/initial.xml"_L1));
            @autoreleasepool {
                weakDelegate = SparkleUpdaterTestController::capturedDelegate();
                weakController = SparkleUpdaterTestController::capturedController();
                updater->setUpdateUrl(QUrl("https://example.test/changed.xml"_L1));
            }
            QVERIFY(weakDelegate != nil);
            QVERIFY(weakController != nil);
            QCOMPARE(QString::fromNSString([weakDelegate feedURLStringForUpdater:weakController.updater]), "https://example.test/changed.xml"_L1);
            QSignalSpy changes(updater.get(), &OCC::SparkleUpdater::statusChanged);
            [weakDelegate updaterDidNotFindUpdate:weakController.updater];
            QCOMPARE(updater->state(), OCC::SparkleUpdater::State::Idle);
            QVERIFY(!updater->statusString().isEmpty());
            QCOMPARE(changes.count(), 1);
        }
        QTRY_VERIFY(weakDelegate == nil);
        QTRY_VERIFY(weakController == nil);
    }

    void externallyRetainedDelegateIgnoresCallbacksAfterOwnerDestruction()
    {
        __weak id<SPUUpdaterDelegate> weakDelegate;
        @autoreleasepool {
            auto updater = std::make_unique<OCC::SparkleUpdater>(QUrl("https://example.test/appcast.xml"_L1));
            id<SPUUpdaterDelegate> const delegate = SparkleUpdaterTestController::capturedDelegate();
            SPUUpdater *const nativeUpdater = SparkleUpdaterTestController::capturedController().updater;
            weakDelegate = delegate;
            updater.reset();
            QVERIFY(weakDelegate != nil);
            [delegate updaterDidNotFindUpdate:nativeUpdater];
            [delegate updater:nativeUpdater didAbortWithError:[NSError errorWithDomain:NSCocoaErrorDomain code:NSUserCancelledError userInfo:nil]];
        }
        QTRY_VERIFY(weakDelegate == nil);
    }

    void missingControllerStillReleasesDelegate()
    {
        SparkleUpdaterTestController::failInitialization(true);
        __weak id<SPUUpdaterDelegate> weakDelegate;
        @autoreleasepool {
            const auto updater = std::make_unique<OCC::SparkleUpdater>(QUrl("https://example.test/appcast.xml"_L1));
            weakDelegate = SparkleUpdaterTestController::capturedDelegate();
            QVERIFY(weakDelegate != nil);
            QVERIFY(SparkleUpdaterTestController::capturedController() == nil);
            updater->setUpdateUrl(QUrl("https://example.test/replacement.xml"_L1));
            QCOMPARE(updater->state(), OCC::SparkleUpdater::State::Unknown);
        }
        QTRY_VERIFY(weakDelegate == nil);
    }
};

QTEST_GUILESS_MAIN(TestSparkleUpdaterMacOS)
#include "testsparkleupdatermacos.moc"
