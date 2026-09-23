/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: CC0-1.0
 *
 * This software is in the public domain, furnished "as is", without technical
 * support, and with no warranty, express or implied, as to its usefulness for
 * any purpose.
 */

#include <QtTest>

#include "archive/anonymizer.h"

using namespace OCC;

class TestAnonymizer : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testUserConsistency()
    {
        Anonymizer anonymizer;
        anonymizer.seedUser(QStringLiteral("username"));
        const auto first = anonymizer.anonymizeText(QStringLiteral("username"));
        const auto second = anonymizer.anonymizeText(QStringLiteral("username"));
        QCOMPARE(first, QStringLiteral("user1"));
        QCOMPARE(second, first);
    }

    void testUsersAreDistinct()
    {
        Anonymizer anonymizer;
        anonymizer.seedUser(QStringLiteral("username"));
        anonymizer.seedUser(QStringLiteral("otheruser"));
        QCOMPARE(anonymizer.anonymizeText(QStringLiteral("username")), QStringLiteral("user1"));
        QCOMPARE(anonymizer.anonymizeText(QStringLiteral("otheruser")), QStringLiteral("user2"));
    }

    void testServerUrlHostRewritten()
    {
        Anonymizer anonymizer;
        anonymizer.seedServer(QStringLiteral("https://cloud.example.com"));
        QCOMPARE(anonymizer.anonymizeText(QStringLiteral("GET https://cloud.example.com/remote.php/dav")),
                 QStringLiteral("GET https://server1.example/remote.php/dav"));
    }

    void testIpv4SequentialLoopback()
    {
        Anonymizer anonymizer;
        const auto scrubbed = anonymizer.anonymizeText(QStringLiteral("from 192.168.1.5 and 192.168.1.5 and 10.9.8.7"));
        QCOMPARE(scrubbed, QStringLiteral("from 127.0.0.1 and 127.0.0.1 and 127.0.0.2"));
    }

    void testIpv6IsRemoved()
    {
        Anonymizer anonymizer;
        const auto scrubbed = anonymizer.anonymizeText(QStringLiteral("peer fe80::1ff:fe23:4567:890a done"));
        QVERIFY(!scrubbed.contains(QLatin1String("fe80::1ff:fe23:4567:890a")));
    }

    void testPathPerSegmentKeepsExtension()
    {
        Anonymizer anonymizer;
        QCOMPARE(anonymizer.anonymizePath(QStringLiteral("Photos/Taxes/Q3 Report.docx")), QStringLiteral("dir_1/dir_2/file_1.docx"));
    }

    void testPathSegmentsAreReused()
    {
        Anonymizer anonymizer;
        QCOMPARE(anonymizer.anonymizePath(QStringLiteral("Photos/a.txt")), QStringLiteral("dir_1/file_1.txt"));
        QCOMPARE(anonymizer.anonymizePath(QStringLiteral("Photos/b.txt")), QStringLiteral("dir_1/file_2.txt"));
    }

    void testAnonymizeTextIsIdempotent()
    {
        Anonymizer anonymizer;
        anonymizer.seedUser(QStringLiteral("username"));
        anonymizer.seedServer(QStringLiteral("https://cloud.example.com"));
        const auto line = QStringLiteral("username on https://cloud.example.com from 192.168.1.5");
        const auto once = anonymizer.anonymizeText(line);
        const auto twice = anonymizer.anonymizeText(once);
        QCOMPARE(twice, once);
    }

    void testNoSeededValueLeaks()
    {
        Anonymizer anonymizer;
        anonymizer.seedUser(QStringLiteral("username"));
        anonymizer.seedServer(QStringLiteral("https://cloud.example.com"));
        anonymizer.seedPath(QStringLiteral("/home/username/Nextcloud"));
        const auto line = QStringLiteral(
            "PROPFIND /home/username/Nextcloud/Photos/IMG.jpg on "
            "https://cloud.example.com by username from 192.168.1.5");
        const auto scrubbed = anonymizer.anonymizeText(line);
        QVERIFY(!scrubbed.contains(QLatin1String("username")));
        QVERIFY(!scrubbed.contains(QLatin1String("cloud.example.com")));
        QVERIFY(!scrubbed.contains(QLatin1String("192.168.1.5")));
    }

    void testMappingIsPopulated()
    {
        Anonymizer anonymizer;
        anonymizer.seedUser(QStringLiteral("username"));
        anonymizer.anonymizeText(QStringLiteral("username"));
        const auto table = anonymizer.mapping();
        QVERIFY(table.contains(QStringLiteral("username")));
        QCOMPARE(table.value(QStringLiteral("username")), QStringLiteral("user1"));
    }
};

QTEST_GUILESS_MAIN(TestAnonymizer)
#include "testanonymizer.moc"
