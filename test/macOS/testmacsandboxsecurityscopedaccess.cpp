/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "common/macsandboxsecurityscopedaccess.h"

#include <QtTest>

#include "securityscopedaccessmock.h"

using namespace Qt::StringLiterals;
using OCC::Utility::MacSandboxSecurityScopedAccess;

class TestMacSandboxSecurityScopedAccess : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<SecurityScopedAccessMock> _native;

    [[nodiscard]] QUrl fileUrl(const QString &name = {}) const
    {
        return _native->fileUrl(name);
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

    void acquiredUrlSurvivesAutoreleasePoolAndStopsOnDestruction()
    {
        std::unique_ptr<MacSandboxSecurityScopedAccess> access;
        @autoreleasepool {
            access = MacSandboxSecurityScopedAccess::create(fileUrl());
        }

        QVERIFY(access->isValid());
        QCOMPARE(_native->startedPaths, QStringList{fileUrl().toLocalFile()});
        QVERIFY(_native->stoppedPaths.isEmpty());
        QCOMPARE(_native->liveUrlCount(), 1);

        access.reset();

        QCOMPARE(_native->stoppedPaths, _native->startedPaths);
        QCOMPARE(_native->liveUrlCount(), 0);
    }

    void deniedAccessReleasesUrlWithoutStopping()
    {
        _native->granted = false;
        std::unique_ptr<MacSandboxSecurityScopedAccess> access;
        @autoreleasepool {
            access = MacSandboxSecurityScopedAccess::create(fileUrl());
        }

        QVERIFY(!access->isValid());
        QCOMPARE(_native->startedPaths, QStringList{fileUrl().toLocalFile()});
        QCOMPARE(_native->liveUrlCount(), 1);

        access.reset();

        QVERIFY(_native->stoppedPaths.isEmpty());
        QCOMPARE(_native->liveUrlCount(), 0);
    }

    void invalidUrls_data()
    {
        QTest::addColumn<QUrl>("url");
        QTest::newRow("empty") << QUrl();
        QTest::newRow("invalid") << QUrl(u"http://["_s, QUrl::StrictMode);
        QTest::newRow("remote") << QUrl(u"https://example.invalid/file"_s);
        QTest::newRow("relative") << QUrl(u"relative-file"_s);
    }

    void invalidUrls()
    {
        QFETCH(QUrl, url);

        const auto access = MacSandboxSecurityScopedAccess::create(url);

        QVERIFY(!access->isValid());
        QVERIFY(_native->startedPaths.isEmpty());
        QVERIFY(_native->stoppedPaths.isEmpty());
        QCOMPARE(_native->liveUrlCount(), 0);
    }

    void missingNativeUrlDoesNotStartAccess()
    {
        _native->missingUrl = true;

        const auto access = MacSandboxSecurityScopedAccess::create(fileUrl());

        QVERIFY(!access->isValid());
        QVERIFY(_native->startedPaths.isEmpty());
        QVERIFY(_native->stoppedPaths.isEmpty());
        QCOMPARE(_native->liveUrlCount(), 0);
    }

    void moveConstructionTransfersAccess()
    {
        std::unique_ptr<MacSandboxSecurityScopedAccess> original;
        @autoreleasepool {
            original = MacSandboxSecurityScopedAccess::create(fileUrl());
        }

        auto moved = std::make_unique<MacSandboxSecurityScopedAccess>(std::move(*original));
        QVERIFY(moved->isValid());
        QVERIFY(!original->isValid());
        original.reset();
        QVERIFY(_native->stoppedPaths.isEmpty());
        QCOMPARE(_native->liveUrlCount(), 1);

        moved.reset();

        QCOMPARE(_native->stoppedPaths, _native->startedPaths);
        QCOMPARE(_native->liveUrlCount(), 0);
    }

    void moveAssignmentStopsPreviousAccessAndTransfersNewAccess()
    {
        std::unique_ptr<MacSandboxSecurityScopedAccess> destination;
        std::unique_ptr<MacSandboxSecurityScopedAccess> source;
        @autoreleasepool {
            destination = MacSandboxSecurityScopedAccess::create(fileUrl(u"previous"_s));
            source = MacSandboxSecurityScopedAccess::create(fileUrl(u"next"_s));
        }
        QCOMPARE(_native->liveUrlCount(), 2);

        *destination = std::move(*source);

        QVERIFY(destination->isValid());
        QVERIFY(!source->isValid());
        QCOMPARE(_native->stoppedPaths, QStringList{fileUrl(u"previous"_s).toLocalFile()});
        QCOMPARE(_native->liveUrlCount(), 1);
        source.reset();
        QCOMPARE(_native->stoppedPaths.size(), 1);

        destination.reset();

        QCOMPARE(_native->stoppedPaths, _native->startedPaths);
        QCOMPARE(_native->liveUrlCount(), 0);
    }
};

QTEST_APPLESS_MAIN(TestMacSandboxSecurityScopedAccess)
#include "testmacsandboxsecurityscopedaccess.moc"
