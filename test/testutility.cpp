/*
   This software is in the public domain, furnished "as is", without technical
   support, and with no warranty, express or implied, as to its usefulness for
   any purpose.
*/

#include <QtTest>
#include <QTemporaryDir>

#include "common/filesystembase.h"
#include "common/utility.h"

#include "libsync/theme.h"

using namespace OCC::Utility;

namespace OCC {
OCSYNC_EXPORT extern bool fsCasePreserving_override;
}

class TestUtility : public QObject
{
    Q_OBJECT

private slots:
    void testFormatFingerprint()
    {
        QVERIFY2(formatFingerprint("68ac906495480a3404beee4874ed853a037a7a8f") == QStringLiteral("68:ac:90:64:95:48:0a:34:04:be:ee:48:74:ed:85:3a:03:7a:7a:8f"),
            "Utility::formatFingerprint() is broken");
    }
    void testOctetsToString()
    {
        QLocale::setDefault(QLocale(QStringLiteral("en")));
        QCOMPARE(octetsToString(999), QString::fromLatin1("999 B"));
        QCOMPARE(octetsToString(1024), QString::fromLatin1("1 KB"));
        QCOMPARE(octetsToString(1364), QString::fromLatin1("1 KB"));

        QCOMPARE(octetsToString(9110), QString::fromLatin1("9 KB"));
        QCOMPARE(octetsToString(9910), QString::fromLatin1("10 KB"));
        QCOMPARE(octetsToString(10240), QString::fromLatin1("10 KB"));

        QCOMPARE(octetsToString(123456), QString::fromLatin1("121 KB"));
        QCOMPARE(octetsToString(1234567), QString::fromLatin1("1.2 MB"));
        QCOMPARE(octetsToString(12345678), QString::fromLatin1("12 MB"));
        QCOMPARE(octetsToString(123456789), QString::fromLatin1("118 MB"));
        QCOMPARE(octetsToString(1000LL * 1000 * 1000 * 5), QString::fromLatin1("4.7 GB"));

        QCOMPARE(octetsToString(1), QString::fromLatin1("1 B"));
        QCOMPARE(octetsToString(2), QString::fromLatin1("2 B"));
        QCOMPARE(octetsToString(1024), QString::fromLatin1("1 KB"));
        QCOMPARE(octetsToString(1024 * 1024), QString::fromLatin1("1 MB"));
        QCOMPARE(octetsToString(1024LL * 1024 * 1024), QString::fromLatin1("1 GB"));
    }

    void testLaunchOnStartup()
    {
        QString postfix = QString::number(QRandomGenerator::global()->generate());

        const QString appName = QStringLiteral("testLaunchOnStartup.%1").arg(postfix);
        const QString guiName = QStringLiteral("LaunchOnStartup GUI Name");

        QVERIFY(hasLaunchOnStartup(appName) == false);
        setLaunchOnStartup(appName, guiName, true);
        QVERIFY(hasLaunchOnStartup(appName) == true);
        setLaunchOnStartup(appName, guiName, false);
        QVERIFY(hasLaunchOnStartup(appName) == false);
    }

    void testDurationToDescriptiveString()
    {
        QLocale::setDefault(QLocale(QStringLiteral("C")));
        //NOTE: in order for the plural to work we would need to load the english translation

        quint64 sec = 1000;
        quint64 hour = 3600 * sec;

        QDateTime current = QDateTime::currentDateTimeUtc();

        QCOMPARE(durationToDescriptiveString2(0), QString::fromLatin1("0 second(s)"));
        QCOMPARE(durationToDescriptiveString2(5), QString::fromLatin1("0 second(s)"));
        QCOMPARE(durationToDescriptiveString2(1000), QString::fromLatin1("1 second(s)"));
        QCOMPARE(durationToDescriptiveString2(1005), QString::fromLatin1("1 second(s)"));
        QCOMPARE(durationToDescriptiveString2(56123), QString::fromLatin1("56 second(s)"));
        QCOMPARE(durationToDescriptiveString2(90 * sec), QString::fromLatin1("1 minute(s) 30 second(s)"));
        QCOMPARE(durationToDescriptiveString2(3 * hour), QString::fromLatin1("3 hour(s)"));
        QCOMPARE(durationToDescriptiveString2(3 * hour + 20 * sec), QString::fromLatin1("3 hour(s)"));
        QCOMPARE(durationToDescriptiveString2(3 * hour + 70 * sec), QString::fromLatin1("3 hour(s) 1 minute(s)"));
        QCOMPARE(durationToDescriptiveString2(3 * hour + 100 * sec), QString::fromLatin1("3 hour(s) 2 minute(s)"));
        QCOMPARE(durationToDescriptiveString2(current.msecsTo(current.addYears(4).addMonths(5).addDays(2).addSecs(23 * 60 * 60))),
            QString::fromLatin1("4 year(s) 5 month(s)"));
        QCOMPARE(durationToDescriptiveString2(current.msecsTo(current.addDays(2).addSecs(23 * 60 * 60))), QString::fromLatin1("2 day(s) 23 hour(s)"));

        QCOMPARE(durationToDescriptiveString1(0), QString::fromLatin1("0 second(s)"));
        QCOMPARE(durationToDescriptiveString1(5), QString::fromLatin1("0 second(s)"));
        QCOMPARE(durationToDescriptiveString1(1000), QString::fromLatin1("1 second(s)"));
        QCOMPARE(durationToDescriptiveString1(1005), QString::fromLatin1("1 second(s)"));
        QCOMPARE(durationToDescriptiveString1(56123), QString::fromLatin1("56 second(s)"));
        QCOMPARE(durationToDescriptiveString1(90 * sec), QString::fromLatin1("2 minute(s)"));
        QCOMPARE(durationToDescriptiveString1(3 * hour), QString::fromLatin1("3 hour(s)"));
        QCOMPARE(durationToDescriptiveString1(3 * hour + 20 * sec), QString::fromLatin1("3 hour(s)"));
        QCOMPARE(durationToDescriptiveString1(3 * hour + 70 * sec), QString::fromLatin1("3 hour(s)"));
        QCOMPARE(durationToDescriptiveString1(3 * hour + 100 * sec), QString::fromLatin1("3 hour(s)"));
        QCOMPARE(
            durationToDescriptiveString1(current.msecsTo(current.addYears(4).addMonths(5).addDays(2).addSecs(23 * 60 * 60))), QString::fromLatin1("4 year(s)"));
        QCOMPARE(durationToDescriptiveString1(current.msecsTo(current.addDays(2).addSecs(23 * 60 * 60))), QString::fromLatin1("3 day(s)"));
    }

    void testVersionOfInstalledBinary()
    {
        // pass the cmd client from our build dir
        // this is a bit inaccurate as it does not test the "real thing"
        // but cmd and gui have the same --version handler by now
        // and cmd works without X in CI
        QString ver = versionOfInstalledBinary(QStringLiteral(OWNCLOUDCMD_BIN_PATH));
        qDebug() << "Version of installed ownCloud Binary: " << ver;
        QVERIFY(!ver.isEmpty());

        QRegularExpression rx(QStringLiteral("ownCloud \\d+\\.\\d+\\.\\d+.*"), QRegularExpression::CaseInsensitiveOption);
        qDebug() << rx.pattern() << rx.match(ver);
        QVERIFY(rx.match(ver).isValid());
    }

    void testTimeAgo()
    {
        // Both times in same timezone
        QDateTime d1 = QDateTime::fromString(QStringLiteral("2015-01-24T09:20:30+01:00"), Qt::ISODate);
        QDateTime d2 = QDateTime::fromString(QStringLiteral("2015-01-23T09:20:30+01:00"), Qt::ISODate);
        QString s = timeAgoInWords(d2, d1);
        QCOMPARE(s, QLatin1String("1 day(s) ago"));

        // Different timezones
        QDateTime earlyTS = QDateTime::fromString(QStringLiteral("2015-01-24T09:20:30+01:00"), Qt::ISODate);
        QDateTime laterTS = QDateTime::fromString(QStringLiteral("2015-01-24T09:20:30-01:00"), Qt::ISODate);
        s = timeAgoInWords(earlyTS, laterTS);
        QCOMPARE(s, QLatin1String("2 hour(s) ago"));

        // 'Now' in whatever timezone
        earlyTS = QDateTime::currentDateTime();
        laterTS = earlyTS;
        s = timeAgoInWords(earlyTS, laterTS );
        QCOMPARE(s, QLatin1String("now"));

        earlyTS = earlyTS.addSecs(-6);
        s = timeAgoInWords(earlyTS, laterTS );
        QCOMPARE(s, QLatin1String("Less than a minute ago"));
    }

    void testFsCasePreserving()
    {
        QVERIFY(isMac() || isWindows() ? fsCasePreserving() : ! fsCasePreserving());
        QScopedValueRollback<bool> scope(OCC::fsCasePreserving_override);
        OCC::fsCasePreserving_override = 1;
        QVERIFY(fsCasePreserving());
        OCC::fsCasePreserving_override = 0;
        QVERIFY(! fsCasePreserving());
    }

    void testFileNamesEqual()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QDir dir2(dir.path());
        QVERIFY(dir2.mkpath(QStringLiteral("1")));
        if( !fsCasePreserving() ) {
            QVERIFY(dir2.mkpath(QStringLiteral("TEST")));
        }
        QVERIFY(dir2.mkpath(QStringLiteral("test/TESTI")));
        QVERIFY(dir2.mkpath(QStringLiteral("TESTI")));

        QString a = dir.path();
        QString b = dir.path();

        QVERIFY(fileNamesEqual(a, b));

        QVERIFY(fileNamesEqual(a + QStringLiteral("/test"), b + QStringLiteral("/test"))); // both exist
        QVERIFY(fileNamesEqual(a + QStringLiteral("/test/TESTI"), b + QStringLiteral("/test/../test/TESTI"))); // both exist

        QScopedValueRollback<bool> scope(OCC::fsCasePreserving_override, true);
        QVERIFY(fileNamesEqual(a + QStringLiteral("/test"), b + QStringLiteral("/TEST"))); // both exist

        QVERIFY(!fileNamesEqual(a + QStringLiteral("/test"), b + QStringLiteral("/test/TESTI"))); // both are different

        dir.remove();
    }

    void testIsChildOf_data()
    {
        QTest::addColumn<QString>("child");
        QTest::addColumn<QString>("parent");
        QTest::addColumn<bool>("output");
        QTest::addColumn<bool>("casePreserving");

        const auto add = [](const QString &child, const QString &parent, bool result, bool casePreserving = true) {
            const auto title = QStringLiteral("CasePreserving %1: %2 is %3 child of %4").arg(casePreserving ? QStringLiteral("yes") : QStringLiteral("no"), child, result ? QString() : QStringLiteral("not"), parent);
            QTest::addRow("%s", qUtf8Printable(title)) << child << parent << result << casePreserving;
        };
        add(QStringLiteral("/A/a"), QStringLiteral("/A"), true);
        add(QStringLiteral("/A/a"), QStringLiteral("/A/a"), true);
        add(QStringLiteral("/A/a"), QStringLiteral("/A/a/"), true);
        add(QStringLiteral("C:/A/a"), QStringLiteral("C:/A"), true);
        add(QStringLiteral("C:/Aa"), QStringLiteral("C:/A"), false);
        add(QStringLiteral("C:/Aa"), QStringLiteral("C:/A"), false);
        add(QStringLiteral("A/a"), QStringLiteral("A"), true);
        add(QStringLiteral("a/a"), QStringLiteral("A"), true, true);
        add(QStringLiteral("a/a"), QStringLiteral("A"), false, false);
        add(QStringLiteral("Aa"), QStringLiteral("A"), false);
        add(QStringLiteral("A/a"), QStringLiteral("A"), true);
        add(QStringLiteral("A/a"), QStringLiteral("A/"), true);
        add(QStringLiteral("ä/a"), QStringLiteral("ä/"), true);
        add(QStringLiteral("Ä/è/a"), QStringLiteral("Ä/è/"), true);
        add(QStringLiteral("Ä/a"), QStringLiteral("Ä/"), true);
        add(QStringLiteral("Aa"), QStringLiteral("A"), false);
        add(QStringLiteral("https://foo/bar"), QStringLiteral("https://foo"), true);
        add(QStringLiteral("https://foo/bar"), QStringLiteral("http://foo"), false);
        add(QStringLiteral("https://foo/bar"), QStringLiteral("http://foo/foo"), false);
    }

    void testIsChildOf()
    {
        const QScopedValueRollback<bool> rollback(OCC::fsCasePreserving_override);
        QFETCH(QString, child);
        QFETCH(QString, parent);
        QFETCH(bool, output);
        QFETCH(bool, casePreserving);
        OCC::fsCasePreserving_override = casePreserving;
        QCOMPARE(OCC::FileSystem::isChildPathOf(child, parent), output);
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
        ;

#define CHECK_NORMALIZE_ETAG(TEST, EXPECT) \
    QCOMPARE(OCC::Utility::normalizeEtag(QStringLiteral(TEST)), QStringLiteral(EXPECT));

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
};

QTEST_GUILESS_MAIN(TestUtility)
#include "testutility.moc"
