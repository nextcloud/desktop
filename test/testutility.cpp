/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2013 ownCloud, Inc.
 * SPDX-License-Identifier: CC0-1.0
 * 
 * This software is in the public domain, furnished "as is", without technical
 * support, and with no warranty, express or implied, as to its usefulness for
 * any purpose.
 */

#include <QtTest>
#include <QTemporaryDir>

#include "common/utility.h"
#include "config.h"
#include "logger.h"

using namespace OCC::Utility;

namespace OCC {
OCSYNC_EXPORT extern bool fsCasePreserving_override;
}

class TestUtility : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);

        QStandardPaths::setTestModeEnabled(true);
    }

    void testFormatFingerprint()
    {
        QVERIFY2(formatFingerprint("68ac906495480a3404beee4874ed853a037a7a8f")
                 == "68:ac:90:64:95:48:0a:34:04:be:ee:48:74:ed:85:3a:03:7a:7a:8f",
		"Utility::formatFingerprint() is broken");
    }
    void testOctetsToString()
    {
        QLocale::setDefault(QLocale("en"));
        QCOMPARE(octetsToString(999) , QStringLiteral("999 B"));
        QCOMPARE(octetsToString(1024) , QStringLiteral("1 KB"));
        QCOMPARE(octetsToString(1110) , QStringLiteral("1 KB"));
        QCOMPARE(octetsToString(1364) , QStringLiteral("1 KB"));

        QCOMPARE(octetsToString(9110) , QStringLiteral("9 KB"));
        QCOMPARE(octetsToString(9910) , QStringLiteral("10 KB"));
        QCOMPARE(octetsToString(9999) , QStringLiteral("10 KB"));
        QCOMPARE(octetsToString(10240) , QStringLiteral("10 KB"));

        QCOMPARE(octetsToString(123456) , QStringLiteral("121 KB"));
        QCOMPARE(octetsToString(1234567) , QStringLiteral("1.2 MB"));
        QCOMPARE(octetsToString(12345678) , QStringLiteral("12 MB"));
        QCOMPARE(octetsToString(123456789) , QStringLiteral("118 MB"));
        QCOMPARE(octetsToString(1000LL*1000*1000 * 5) , QStringLiteral("4.7 GB"));

        QCOMPARE(octetsToString(1), QStringLiteral("1 B"));
        QCOMPARE(octetsToString(2), QStringLiteral("2 B"));
        QCOMPARE(octetsToString(1024), QStringLiteral("1 KB"));
        QCOMPARE(octetsToString(1024*1024), QStringLiteral("1 MB"));
        QCOMPARE(octetsToString(1024LL*1024*1024), QStringLiteral("1 GB"));
        QCOMPARE(octetsToString(1024LL*1024*1024*1024), QStringLiteral("1 TB"));
        QCOMPARE(octetsToString(1024LL*1024*1024*1024 * 5), QStringLiteral("5 TB"));
    }

    void testLaunchOnStartup()
    {
        QString postfix = QString::number(OCC::Utility::rand());

        const QString appName = QString::fromLatin1("testLaunchOnStartup.%1").arg(postfix);
        const QString guiName = "LaunchOnStartup GUI Name";

        QVERIFY(hasLaunchOnStartup(appName) == false);
        setLaunchOnStartup(appName, guiName, true);
        QVERIFY(hasLaunchOnStartup(appName) == true);
        setLaunchOnStartup(appName, guiName, false);
        QVERIFY(hasLaunchOnStartup(appName) == false);
    }

    void testDurationToDescriptiveString()
    {
        QLocale::setDefault(QLocale("C"));
        //NOTE: in order for the plural to work we would need to load the english translation

        quint64 sec = 1000;
        quint64 hour = 3600 * sec;

        QDateTime current = QDateTime::currentDateTimeUtc();

        QCOMPARE(durationToDescriptiveString2(0), QStringLiteral("0 second(s)") );
        QCOMPARE(durationToDescriptiveString2(5), QStringLiteral("0 second(s)") );
        QCOMPARE(durationToDescriptiveString2(1000), QStringLiteral("1 second(s)") );
        QCOMPARE(durationToDescriptiveString2(1005), QStringLiteral("1 second(s)") );
        QCOMPARE(durationToDescriptiveString2(56123), QStringLiteral("56 second(s)") );
        QCOMPARE(durationToDescriptiveString2(90*sec), QStringLiteral("1 minute(s) 30 second(s)") );
        QCOMPARE(durationToDescriptiveString2(3*hour), QStringLiteral("3 hour(s)") );
        QCOMPARE(durationToDescriptiveString2(3*hour + 20*sec), QStringLiteral("3 hour(s)") );
        QCOMPARE(durationToDescriptiveString2(3*hour + 70*sec), QStringLiteral("3 hour(s) 1 minute(s)") );
        QCOMPARE(durationToDescriptiveString2(3*hour + 100*sec), QStringLiteral("3 hour(s) 2 minute(s)") );
        QCOMPARE(durationToDescriptiveString2(current.msecsTo(current.addYears(4).addMonths(5).addDays(2).addSecs(23*60*60))),
                 QStringLiteral("4 year(s) 5 month(s)") );
        QCOMPARE(durationToDescriptiveString2(current.msecsTo(current.addDays(2).addSecs(23*60*60))),
                 QStringLiteral("2 day(s) 23 hour(s)") );

        QCOMPARE(durationToDescriptiveString1(0), QStringLiteral("0 second(s)") );
        QCOMPARE(durationToDescriptiveString1(5), QStringLiteral("0 second(s)") );
        QCOMPARE(durationToDescriptiveString1(1000), QStringLiteral("1 second(s)") );
        QCOMPARE(durationToDescriptiveString1(1005), QStringLiteral("1 second(s)") );
        QCOMPARE(durationToDescriptiveString1(56123), QStringLiteral("56 second(s)") );
        QCOMPARE(durationToDescriptiveString1(90*sec), QStringLiteral("2 minute(s)") );
        QCOMPARE(durationToDescriptiveString1(3*hour), QStringLiteral("3 hour(s)") );
        QCOMPARE(durationToDescriptiveString1(3*hour + 20*sec), QStringLiteral("3 hour(s)") );
        QCOMPARE(durationToDescriptiveString1(3*hour + 70*sec), QStringLiteral("3 hour(s)") );
        QCOMPARE(durationToDescriptiveString1(3*hour + 100*sec), QStringLiteral("3 hour(s)") );
        QCOMPARE(durationToDescriptiveString1(current.msecsTo(current.addYears(4).addMonths(5).addDays(2).addSecs(23*60*60))),
                 QStringLiteral("4 year(s)") );
        QCOMPARE(durationToDescriptiveString1(current.msecsTo(current.addDays(2).addSecs(23*60*60))),
                 QStringLiteral("3 day(s)") );

    }

    void testVersionOfInstalledBinary()
    {
        if (isLinux()) {
            // pass the cmd client from our build dir
            // this is a bit inaccurate as it does not test the "real thing"
            // but cmd and gui have the same --version handler by now
            // and cmd works without X in CI
            QString ver = versionOfInstalledBinary(QStringLiteral(OWNCLOUD_BIN_PATH  "/" APPLICATION_EXECUTABLE "cmd"));
            qDebug() << "Version of installed Nextcloud: " << ver;
            QVERIFY(!ver.isEmpty());

            const QRegularExpression rx(QRegularExpression::anchoredPattern(APPLICATION_SHORTNAME R"( version \d+\.\d+\.\d+.*)"));
            QVERIFY(rx.match(ver).hasMatch());
        } else {
            QVERIFY(versionOfInstalledBinary().isEmpty());
        }
    }

    void testTimeAgo()
    {
        // Both times in same timezone
        QDateTime d1 = QDateTime::fromString("2015-01-24T09:20:30+01:00", Qt::ISODate);
        QDateTime d2 = QDateTime::fromString("2015-01-23T09:20:30+01:00", Qt::ISODate);
        QString s = timeAgoInWords(d2, d1);
        QCOMPARE(s, QLatin1String("1d"));

        // Different timezones
        QDateTime earlyTS = QDateTime::fromString("2015-01-24T09:20:30+01:00", Qt::ISODate);
        QDateTime laterTS = QDateTime::fromString("2015-01-24T09:20:30-01:00", Qt::ISODate);
        s = timeAgoInWords(earlyTS, laterTS);
        QCOMPARE(s, QLatin1String("2h"));

        // 'Now' in whatever timezone
        earlyTS = QDateTime::currentDateTime();
        laterTS = earlyTS;
        s = timeAgoInWords(earlyTS, laterTS );
        QCOMPARE(s, QLatin1String("now"));

        earlyTS = earlyTS.addSecs(-6);
        s = timeAgoInWords(earlyTS, laterTS );
        QCOMPARE(s, QLatin1String("1min"));
    }

    void testFsCasePreserving()
    {
        QVERIFY(isMac() || isWindows() ? fsCasePreserving() : ! fsCasePreserving());
        QScopedValueRollback<bool> scope(OCC::fsCasePreserving_override);
        OCC::fsCasePreserving_override = true;
        QVERIFY(fsCasePreserving());
        OCC::fsCasePreserving_override = false;
        QVERIFY(! fsCasePreserving());
    }

    void testFileNamesEqual()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QDir dir2(dir.path());
        QVERIFY(dir2.mkpath("test"));
        if( !fsCasePreserving() ) {
        QVERIFY(dir2.mkpath("TEST"));
        }
        QVERIFY(dir2.mkpath("test/TESTI"));
        QVERIFY(dir2.mkpath("TESTI"));

        QString a = dir.path();
        QString b = dir.path();

        QVERIFY(fileNamesEqual(a, b));

        QVERIFY(fileNamesEqual(a+"/test", b+"/test")); // both exist
        QVERIFY(fileNamesEqual(a+"/test/TESTI", b+"/test/../test/TESTI")); // both exist

        QScopedValueRollback<bool> scope(OCC::fsCasePreserving_override, true);
        QVERIFY(fileNamesEqual(a+"/test", b+"/TEST")); // both exist

        QVERIFY(!fileNamesEqual(a+"/test", b+"/test/TESTI")); // both are different

        dir.remove();
    }

    void testSanitizeForFileName_data()
    {
        QTest::addColumn<QString>("input");
        QTest::addColumn<QString>("output");

        QTest::newRow("")
            << "foobar"
            << "foobar";
        QTest::newRow("")
            << "a/b?c<d>e\\f:g*h|i\"j"
            << "abcdefghij";
        QTest::newRow("")
            << QString::fromLatin1("a\x01 b\x1f c\x80 d\x9f")
            << "a b c d";
    }

    void testSanitizeForFileName()
    {
        QFETCH(QString, input);
        QFETCH(QString, output);
        QCOMPARE(sanitizeForFileName(input), output);
    }

    void testNormalizeEtag()
    {
        QByteArray str;

#define CHECK_NORMALIZE_ETAG(TEST, EXPECT) \
    str = OCC::Utility::normalizeEtag(TEST); \
    QCOMPARE(str.constData(), EXPECT); \

        CHECK_NORMALIZE_ETAG("foo", "foo");
        CHECK_NORMALIZE_ETAG("\"foo\"", "foo");
        CHECK_NORMALIZE_ETAG("\"nar123\"", "nar123");
        CHECK_NORMALIZE_ETAG("", "");
        CHECK_NORMALIZE_ETAG("\"\"", "");

        /* Test with -gzip (all combinaison) */
        CHECK_NORMALIZE_ETAG("foo-gzip", "foo");
        CHECK_NORMALIZE_ETAG("\"foo\"-gzip", "foo");
        CHECK_NORMALIZE_ETAG("\"foo-gzip\"", "foo");
    }

    void testIsPathWindowsDrivePartitionRoot()
    {
#ifdef Q_OS_WIN
        // a non-root of a Windows partition
        QVERIFY(!isPathWindowsDrivePartitionRoot("c:/a"));
        QVERIFY(!isPathWindowsDrivePartitionRoot("c:\\a"));

        // a root of a Windows partition (c, d, e)
        QVERIFY(isPathWindowsDrivePartitionRoot("c:"));
        QVERIFY(isPathWindowsDrivePartitionRoot("c:/"));
        QVERIFY(isPathWindowsDrivePartitionRoot("c:\\"));

        QVERIFY(isPathWindowsDrivePartitionRoot("d:"));
        QVERIFY(isPathWindowsDrivePartitionRoot("d:/"));
        QVERIFY(isPathWindowsDrivePartitionRoot("d:\\"));

        QVERIFY(isPathWindowsDrivePartitionRoot("e:"));
        QVERIFY(isPathWindowsDrivePartitionRoot("e:/"));
        QVERIFY(isPathWindowsDrivePartitionRoot("e:\\"));

        // a single character
        QVERIFY(!isPathWindowsDrivePartitionRoot("a"));

        // a missing second character
        QVERIFY(!isPathWindowsDrivePartitionRoot("c/"));
        QVERIFY(!isPathWindowsDrivePartitionRoot("c\\"));

        // an incorrect second character
        QVERIFY(!isPathWindowsDrivePartitionRoot("c;"));
        QVERIFY(!isPathWindowsDrivePartitionRoot("c;/"));
        QVERIFY(!isPathWindowsDrivePartitionRoot("c;\\"));

        // a non-missing, but, incorrect last character
        QVERIFY(!isPathWindowsDrivePartitionRoot("c:!"));

        // an incorrect path length
        QVERIFY(!isPathWindowsDrivePartitionRoot("cd:"));
        QVERIFY(!isPathWindowsDrivePartitionRoot("cd:/"));
        QVERIFY(!isPathWindowsDrivePartitionRoot("cd:\\"));

        // a non-alphabetic first character
        QVERIFY(!isPathWindowsDrivePartitionRoot("0:"));
        QVERIFY(!isPathWindowsDrivePartitionRoot("0:/"));
        QVERIFY(!isPathWindowsDrivePartitionRoot("0:\\"));
#else
        // should always return false on non-Windows
        QVERIFY(!isPathWindowsDrivePartitionRoot("c:"));
        QVERIFY(!isPathWindowsDrivePartitionRoot("c:/"));
        QVERIFY(!isPathWindowsDrivePartitionRoot("c:\\"));
#endif
    }

    void testFullRemotePathToRemoteSyncRootRelative()
    {
        QVector<QPair<QString, QString>> remoteFullPathsForRoot = {
            {"2020", {"2020"}},
            {"/2021/", {"2021"}},
            {"/2022/file.docx", {"2022/file.docx"}}
        };
        // test against root remote path - result must stay unchanged, leading and trailing slashes must get removed
        for (const auto &remoteFullPathForRoot : remoteFullPathsForRoot) {
            const auto fullRemotePathOriginal = remoteFullPathForRoot.first;
            const auto fullRemotePathExpected = remoteFullPathForRoot.second;
            const auto fullRepotePathResult = OCC::Utility::fullRemotePathToRemoteSyncRootRelative(fullRemotePathOriginal, "/");
            QCOMPARE(fullRepotePathResult, fullRemotePathExpected);
        }

        const auto remotePathNonRoot = QStringLiteral("/Documents/reports");
        QVector<QPair<QString, QString>> remoteFullPathsForNonRoot = {
            {remotePathNonRoot + "/" + "2020", {"2020"}},
            {remotePathNonRoot + "/" + "2021/", {"2021"}},
            {remotePathNonRoot + "/" + "2022/file.docx", {"2022/file.docx"}}
        };

        // test against non-root remote path - must always return a proper path as in local db
        for (const auto &remoteFullPathForNonRoot : remoteFullPathsForNonRoot) {
            const auto fullRemotePathOriginal = remoteFullPathForNonRoot.first;
            const auto fullRemotePathExpected = remoteFullPathForNonRoot.second;
            const auto fullRepotePathResult = OCC::Utility::fullRemotePathToRemoteSyncRootRelative(fullRemotePathOriginal, remotePathNonRoot);
            QCOMPARE(fullRepotePathResult, fullRemotePathExpected);
        }

        // test against non-root remote path with trailing slash - must work the same
        const auto remotePathNonRootWithTrailingSlash = QStringLiteral("/Documents/reports/");
        for (const auto &remoteFullPathForNonRoot : remoteFullPathsForNonRoot) {
            const auto fullRemotePathOriginal = remoteFullPathForNonRoot.first;
            const auto fullRemotePathExpected = remoteFullPathForNonRoot.second;
            const auto fullRepotePathResult = OCC::Utility::fullRemotePathToRemoteSyncRootRelative(fullRemotePathOriginal, remotePathNonRootWithTrailingSlash);
            QCOMPARE(fullRepotePathResult, fullRemotePathExpected);
        }

        // test against unrelated remote path - result must stay unchanged
        const auto remotePathUnrelated = QStringLiteral("/Documents1/reports");
        for (const auto &remoteFullPathForNonRoot : remoteFullPathsForNonRoot) {
            const auto fullRemotePathOriginal = remoteFullPathForNonRoot.first;
            const auto fullRepotePathResult = OCC::Utility::fullRemotePathToRemoteSyncRootRelative(fullRemotePathOriginal, remotePathUnrelated);
            QCOMPARE(fullRepotePathResult, fullRemotePathOriginal);
        }
    }

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    void testSetupFavLink()
    {
        // avoid polluting user bookmarks
        const auto originalHome = qgetenv("HOME");
        QTemporaryDir tempHome;
        qputenv("HOME", tempHome.path().toLocal8Bit());
        QCOMPARE(QDir::homePath(), tempHome.path());

        QDir(tempHome.path()).mkpath(".config/gtk-3.0");
        const auto bookmarksFilePath = tempHome.filePath(".config/gtk-3.0/bookmarks");

        const auto countEntries = [&bookmarksFilePath](const QString &expectedEntry) -> qsizetype {
            QFile gtkBookmarks(bookmarksFilePath);
            if (!gtkBookmarks.open(QFile::ReadOnly)) {
                qCritical() << "opening file" << bookmarksFilePath << "failed with" << gtkBookmarks.errorString();
                return -1;
            }

            const auto places = gtkBookmarks.readAll().split('\n');
            return places.count(expectedEntry);
        };

        const auto setupAndCheckFavLink = [&countEntries](const QString &folder, const QString &expectedEntry) -> void {
            OCC::Utility::setupFavLink(folder);
            QCOMPARE(countEntries(expectedEntry), 1);
            // ensure duplicates aren't created
            OCC::Utility::setupFavLink(folder);
            QCOMPARE(countEntries(expectedEntry), 1);
        };

        setupAndCheckFavLink("/tmp/test", "file:///tmp/test");
        setupAndCheckFavLink("/tmp/test with spaces", "file:///tmp/test%20with%20spaces");
        setupAndCheckFavLink("/tmp/special! characters & more/subpath with space", "file:///tmp/special!%20characters%20&%20more/subpath%20with%20space");

        // restore defaults for other tests
        qputenv("HOME", originalHome);
        QCOMPARE(QDir::homePath(), originalHome);
    }
#endif
};

QTEST_GUILESS_MAIN(TestUtility)
#include "testutility.moc"
