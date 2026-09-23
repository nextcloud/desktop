/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: CC0-1.0
 *
 * This software is in the public domain, furnished "as is", without technical
 * support, and with no warranty, express or implied, as to its usefulness for
 * any purpose.
 */

#include <QTemporaryDir>
#include <QtTest>

#include <zlib.h>

#include "archive/anonymizer.h"
#include "archive/debugarchiveanonymizer.h"

using namespace OCC;

namespace
{
bool writeText(const QString &path, const QString &content)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    return file.write(content.toUtf8()) == content.toUtf8().size();
}

QString readText(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

bool writeGzip(const QString &path, const QString &content)
{
    const auto gzipFile = gzopen(path.toUtf8().constData(), "wb");
    if (!gzipFile) {
        return false;
    }
    const auto utf8Bytes = content.toUtf8();
    const auto bytesWritten = gzwrite(gzipFile, utf8Bytes.constData(), static_cast<unsigned>(utf8Bytes.size()));
    gzclose(gzipFile);
    return bytesWritten == static_cast<int>(utf8Bytes.size());
}
}

class TestDebugArchiveAnonymizer : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testAnonymizePlainLog()
    {
        QTemporaryDir temporaryDir;
        QVERIFY(temporaryDir.isValid());
        const auto sourcePath = temporaryDir.filePath(QStringLiteral("nextcloud.log"));
        const auto destinationPath = temporaryDir.filePath(QStringLiteral("out.log"));
        QVERIFY(writeText(sourcePath, QStringLiteral("sync for username at https://cloud.example.com from 192.168.1.5\n")));

        Anonymizer anonymizer;
        anonymizer.seedUser(QStringLiteral("username"));
        anonymizer.seedServer(QStringLiteral("https://cloud.example.com"));

        QVERIFY(DebugArchiveAnonymizer::anonymizeLogFile(anonymizer, sourcePath, destinationPath));
        const auto outputText = readText(destinationPath);
        QVERIFY(!outputText.contains(QLatin1String("username")));
        QVERIFY(!outputText.contains(QLatin1String("cloud.example.com")));
        QVERIFY(!outputText.contains(QLatin1String("192.168.1.5")));
        QVERIFY(outputText.contains(QLatin1String("user1")));
        QVERIFY(outputText.contains(QLatin1String("server1.example")));
        QVERIFY(outputText.contains(QLatin1String("127.0.0.1")));
    }

    void testAnonymizeGzipLog()
    {
        QTemporaryDir temporaryDir;
        QVERIFY(temporaryDir.isValid());
        const auto sourcePath = temporaryDir.filePath(QStringLiteral("nextcloud.log.1.gz"));
        const auto destinationPath = temporaryDir.filePath(QStringLiteral("out.log"));
        QVERIFY(writeGzip(sourcePath, QStringLiteral("rotated line for username at https://cloud.example.com\n")));

        Anonymizer anonymizer;
        anonymizer.seedUser(QStringLiteral("username"));
        anonymizer.seedServer(QStringLiteral("https://cloud.example.com"));

        QVERIFY(DebugArchiveAnonymizer::anonymizeLogFile(anonymizer, sourcePath, destinationPath));
        const auto outputText = readText(destinationPath);
        QVERIFY(!outputText.contains(QLatin1String("username")));
        QVERIFY(!outputText.contains(QLatin1String("cloud.example.com")));
        QVERIFY(outputText.contains(QLatin1String("user1")));
        QVERIFY(outputText.contains(QLatin1String("server1.example")));
    }

    void testConfigDropsCredentialsAndPseudonymizes()
    {
        QTemporaryDir temporaryDir;
        QVERIFY(temporaryDir.isValid());
        const auto sourcePath = temporaryDir.filePath(QStringLiteral("nextcloud.cfg"));
        const auto destinationPath = temporaryDir.filePath(QStringLiteral("out.cfg"));
        const auto content = QStringLiteral(
            "[Accounts]\n"
            "0\\url=https://cloud.example.com\n"
            "0\\dav_user=username\n"
            "0\\password=SuperSecret123\n"
            "0\\clientSecret=abcdef\n"
            "0\\http_oauth_access_token=zzz\n");
        QVERIFY(writeText(sourcePath, content));

        Anonymizer anonymizer;
        anonymizer.seedUser(QStringLiteral("username"));
        anonymizer.seedServer(QStringLiteral("https://cloud.example.com"));

        QVERIFY(DebugArchiveAnonymizer::anonymizeConfigFile(anonymizer, sourcePath, destinationPath));
        const auto outputText = readText(destinationPath);
        QVERIFY(outputText.contains(QLatin1String("server1.example")));
        QVERIFY(outputText.contains(QLatin1String("user1")));
        QVERIFY(!outputText.contains(QLatin1String("username")));
        QVERIFY(!outputText.contains(QLatin1String("cloud.example.com")));
        QVERIFY(!outputText.contains(QLatin1String("SuperSecret123")));
        QVERIFY(!outputText.contains(QLatin1String("abcdef")));
        QVERIFY(!outputText.contains(QLatin1String("zzz")));
    }

    void testWriteLookupTable()
    {
        QTemporaryDir temporaryDir;
        QVERIFY(temporaryDir.isValid());
        const auto destinationPath = temporaryDir.filePath(QStringLiteral("archive-lookup.txt"));

        Anonymizer anonymizer;
        anonymizer.seedUser(QStringLiteral("username"));
        anonymizer.anonymizeText(QStringLiteral("username"));

        QVERIFY(DebugArchiveAnonymizer::writeLookupTable(anonymizer, destinationPath));
        const auto outputText = readText(destinationPath);
        QVERIFY(outputText.contains(QLatin1String("username")));
        QVERIFY(outputText.contains(QLatin1String("user1")));
    }

    void testMissingSourceFails()
    {
        QTemporaryDir temporaryDir;
        QVERIFY(temporaryDir.isValid());
        Anonymizer anonymizer;
        QVERIFY(!DebugArchiveAnonymizer::anonymizeLogFile(anonymizer,
                                                          temporaryDir.filePath(QStringLiteral("does-not-exist.log")),
                                                          temporaryDir.filePath(QStringLiteral("out.log"))));
    }
};

QTEST_GUILESS_MAIN(TestDebugArchiveAnonymizer)
#include "testdebugarchiveanonymizer.moc"
