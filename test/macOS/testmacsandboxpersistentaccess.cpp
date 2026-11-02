/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "common/macsandboxpersistentaccess.h"
#include "securityscopedaccessmock.h"

#include <QtTest>

using namespace Qt::StringLiterals;
using OCC::Utility::MacSandboxPersistentAccess;

class TestMacSandboxPersistentAccess : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<SecurityScopedAccessMock> _native;

    [[nodiscard]] std::unique_ptr<MacSandboxPersistentAccess> createAccess() const
    {
        @autoreleasepool {
            return MacSandboxPersistentAccess::createFromBookmarkData(_native->bookmarkData);
        }
    }

    [[nodiscard]] std::unique_ptr<MacSandboxPersistentAccess> createValidAccess() const
    {
        @autoreleasepool {
            return MacSandboxPersistentAccess::createValidFromBookmarkData(_native->bookmarkData);
        }
    }

private Q_SLOTS:
    void init()
    {
        _native = std::make_unique<SecurityScopedAccessMock>();
        QVERIFY(_native->install());
    }

    void cleanup()
    {
        _native.reset();
    }

    void resolvesBookmarkAndRetainsUrlAcrossAutoreleasePool_data()
    {
        QTest::addColumn<bool>("stale");
        QTest::newRow("fresh") << false;
        QTest::newRow("stale") << true;
    }

    void resolvesBookmarkAndRetainsUrlAcrossAutoreleasePool()
    {
        QFETCH(bool, stale);
        _native->stale = stale;
        _native->resolvedPath = _native->fileUrl(u"sync files/資料"_s).toLocalFile();

        auto access = createAccess();

        QVERIFY(access);
        QVERIFY(access->isValid());
        QCOMPARE(access->isStale(), stale);
        QCOMPARE(_native->dataConversionCount, 1);
        QCOMPARE(_native->resolutionCount, 1);
        QCOMPARE(_native->resolvedBookmark, _native->bookmarkData);
        QCOMPARE(_native->resolutionOptions, NSURLBookmarkResolutionWithSecurityScope);
        QVERIFY(_native->relativeUrlWasNil);
        QVERIFY(_native->staleOutputProvided);
        QVERIFY(_native->errorOutputProvided);
        QCOMPARE(_native->startedPaths, QStringList{_native->resolvedPath});
        QVERIFY(_native->stoppedPaths.isEmpty());
        QCOMPARE(_native->liveUrlCount(), 1);

        access.reset();

        QCOMPARE(_native->stoppedPaths, _native->startedPaths);
        QCOMPARE(_native->liveUrlCount(), 0);
    }

    void deniedAccessReleasesUrlWithoutStopping()
    {
        _native->granted = false;
        auto access = createAccess();

        QVERIFY(access);
        QVERIFY(!access->isValid());
        QVERIFY(!access->isStale());
        QCOMPARE(_native->startedPaths, QStringList{_native->resolvedPath});
        QCOMPARE(_native->liveUrlCount(), 1);

        access.reset();

        QVERIFY(_native->stoppedPaths.isEmpty());
        QCOMPARE(_native->liveUrlCount(), 0);
    }

    void emptyBookmarkIsRejectedByBothFactories()
    {
        _native->bookmarkData.clear();

        QVERIFY(!createAccess());
        QVERIFY(!createValidAccess());
        QCOMPARE(_native->dataConversionCount, 0);
        QCOMPARE(_native->resolutionCount, 0);
        QVERIFY(_native->startedPaths.isEmpty());
        QVERIFY(_native->stoppedPaths.isEmpty());
        QCOMPARE(_native->liveUrlCount(), 0);
    }

    void missingNativeDataDoesNotResolveBookmark()
    {
        _native->missingData = true;

        const auto access = createAccess();

        QVERIFY(access);
        QVERIFY(!access->isValid());
        QVERIFY(!access->isStale());
        QCOMPARE(_native->dataConversionCount, 1);
        QCOMPARE(_native->resolutionCount, 0);
        QVERIFY(_native->startedPaths.isEmpty());
        QVERIFY(_native->stoppedPaths.isEmpty());
        QCOMPARE(_native->liveUrlCount(), 0);
    }

    void failedResolutionReleasesAnyReturnedUrl_data()
    {
        QTest::addColumn<bool>("returnsNil");
        QTest::addColumn<bool>("error");
        QTest::newRow("nil-url") << true << false;
        QTest::newRow("error-and-nil-url") << true << true;
        QTest::newRow("error-and-url") << false << true;
    }

    void failedResolutionReleasesAnyReturnedUrl()
    {
        QFETCH(bool, returnsNil);
        QFETCH(bool, error);
        _native->resolutionReturnsNil = returnsNil;
        _native->resolutionError = error;

        auto access = createAccess();

        QVERIFY(access);
        QVERIFY(!access->isValid());
        QVERIFY(!access->isStale());
        QCOMPARE(_native->liveUrlCount(), returnsNil ? 0 : 1);
        QVERIFY(_native->startedPaths.isEmpty());

        access.reset();

        QCOMPARE(_native->liveUrlCount(), 0);
        QVERIFY(!createValidAccess());
        QCOMPARE(_native->resolutionCount, 2);
        QVERIFY(_native->startedPaths.isEmpty());
        QVERIFY(_native->stoppedPaths.isEmpty());
        QCOMPARE(_native->liveUrlCount(), 0);
    }

    void validFactoryRetainsBookmarkAccess_data()
    {
        QTest::addColumn<bool>("stale");
        QTest::newRow("fresh") << false;
        QTest::newRow("stale") << true;
    }

    void validFactoryRetainsBookmarkAccess()
    {
        QFETCH(bool, stale);
        _native->stale = stale;

        auto access = createValidAccess();

        QVERIFY(access);
        QVERIFY(access->isValid());
        QCOMPARE(access->isStale(), stale);
        QCOMPARE(_native->startedPaths, QStringList{_native->resolvedPath});
        QVERIFY(_native->stoppedPaths.isEmpty());
        QCOMPARE(_native->liveUrlCount(), 1);

        access.reset();

        QCOMPARE(_native->stoppedPaths, _native->startedPaths);
        QCOMPARE(_native->liveUrlCount(), 0);
    }

    void validFactoryRejectsDeniedAccessAndReleasesUrl()
    {
        _native->granted = false;

        QVERIFY(!createValidAccess());

        QCOMPARE(_native->startedPaths, QStringList{_native->resolvedPath});
        QVERIFY(_native->stoppedPaths.isEmpty());
        QCOMPARE(_native->liveUrlCount(), 0);
    }

    void moveConstructionTransfersAccessAndStaleState()
    {
        _native->stale = true;
        auto original = createAccess();
        QVERIFY(original);
        QVERIFY(original->isValid());

        auto moved = std::make_unique<MacSandboxPersistentAccess>(std::move(*original));

        QVERIFY(moved->isValid());
        QVERIFY(moved->isStale());
        QVERIFY(!original->isValid());
        QVERIFY(!original->isStale());
        original.reset();
        QVERIFY(_native->stoppedPaths.isEmpty());
        QCOMPARE(_native->liveUrlCount(), 1);

        moved.reset();

        QCOMPARE(_native->stoppedPaths, _native->startedPaths);
        QCOMPARE(_native->liveUrlCount(), 0);
    }

    void moveAssignmentStopsPreviousAccessAndTransfersNewAccess()
    {
        _native->resolvedPath = _native->fileUrl(u"previous"_s).toLocalFile();
        auto destination = createAccess();
        QVERIFY(destination);
        QVERIFY(destination->isValid());
        _native->resolvedPath = _native->fileUrl(u"next"_s).toLocalFile();
        _native->stale = true;
        auto source = createAccess();
        QVERIFY(source);
        QVERIFY(source->isValid());
        QCOMPARE(_native->liveUrlCount(), 2);

        *destination = std::move(*source);

        QVERIFY(destination->isValid());
        QVERIFY(destination->isStale());
        QVERIFY(!source->isValid());
        QVERIFY(!source->isStale());
        QCOMPARE(_native->stoppedPaths, QStringList{_native->fileUrl(u"previous"_s).toLocalFile()});
        QCOMPARE(_native->liveUrlCount(), 1);
        source.reset();
        QCOMPARE(_native->stoppedPaths.size(), 1);

        destination.reset();

        QCOMPARE(_native->stoppedPaths, _native->startedPaths);
        QCOMPARE(_native->liveUrlCount(), 0);
    }
};

QTEST_APPLESS_MAIN(TestMacSandboxPersistentAccess)
#include "testmacsandboxpersistentaccess.moc"
