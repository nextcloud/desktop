/*
 * libcsync -- a library to sync a directory with another
 *
 * SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2013 ownCloud, Inc.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "common/filesystembase.h"
#include "csync/csync.h"
#include "csync/vio/csync_vio_local.h"
#include "logger.h"

#include <QTemporaryFile>
#include <QTest>
#include <QStandardPaths>

class TestLongWindowsPath : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);

        QStandardPaths::setTestModeEnabled(true);
    }

#ifdef Q_OS_WIN
    void check_long_win_path()
    {
        {
            const auto path = QStringLiteral("C://DATA/FILES/MUSIC/MY_MUSIC.mp3"); // check a short path
            const auto exp_path = QStringLiteral("\\\\?\\C:\\\\DATA\\FILES\\MUSIC\\MY_MUSIC.mp3");
            QString new_short = OCC::FileSystem::pathtoUNC(path);
            QCOMPARE(new_short, exp_path);
        }

        {
            const auto path = QStringLiteral("\\\\foo\\bar/MY_MUSIC.mp3");
            const auto exp_path = QStringLiteral("\\\\foo\\bar\\MY_MUSIC.mp3");
            QString new_short = OCC::FileSystem::pathtoUNC(path);
            QCOMPARE(new_short, exp_path);
        }

        {
            const auto path = QStringLiteral("//foo\\bar/MY_MUSIC.mp3");
            const auto exp_path = QStringLiteral("\\\\foo\\bar\\MY_MUSIC.mp3");
            QString new_short = OCC::FileSystem::pathtoUNC(path);
            QCOMPARE(new_short, exp_path);
        }

        {
            const auto path = QStringLiteral("\\foo\\bar");
            const auto exp_path = QStringLiteral("\\\\?\\foo\\bar");
            QString new_short = OCC::FileSystem::pathtoUNC(path);
            QCOMPARE(new_short, exp_path);
        }

        {
            const auto path = QStringLiteral("/foo/bar");
            const auto exp_path = QStringLiteral("\\\\?\\foo\\bar");
            QString new_short = OCC::FileSystem::pathtoUNC(path);
            QCOMPARE(new_short, exp_path);
        }

        const auto longPath = QStringLiteral("D://alonglonglonglong/blonglonglonglong/clonglonglonglong/dlonglonglonglong/"
                                             "elonglonglonglong/flonglonglonglong/glonglonglonglong/hlonglonglonglong/ilonglonglonglong/"
                                             "jlonglonglonglong/klonglonglonglong/llonglonglonglong/mlonglonglonglong/nlonglonglonglong/"
                                             "olonglonglonglong/file.txt");
        const auto longPathConv = QStringLiteral("\\\\?\\D:\\\\alonglonglonglong\\blonglonglonglong\\clonglonglonglong\\dlonglonglonglong\\"
                                                 "elonglonglonglong\\flonglonglonglong\\glonglonglonglong\\hlonglonglonglong\\ilonglonglonglong\\"
                                                 "jlonglonglonglong\\klonglonglonglong\\llonglonglonglong\\mlonglonglonglong\\nlonglonglonglong\\"
                                                 "olonglonglonglong\\file.txt");

        QString new_long = OCC::FileSystem::pathtoUNC(longPath);
        // printf( "XXXXXXXXXXXX %s %d\n", new_long, mem_reserved);

        QCOMPARE(new_long, longPathConv);

        // printf( "YYYYYYYYYYYY %ld\n", strlen(new_long));
        QCOMPARE(new_long.length(), 286);
    }
#endif


    void testLongPathStat_data()
    {
        QTest::addColumn<QString>("name");

        QTest::newRow("long") << QStringLiteral("/alonglonglonglong/blonglonglonglong/clonglonglonglong/dlonglonglonglong/"
                                                "elonglonglonglong/flonglonglonglong/glonglonglonglong/hlonglonglonglong/ilonglonglonglong/"
                                                "jlonglonglonglong/klonglonglonglong/llonglonglonglong/mlonglonglonglong/nlonglonglonglong/"
                                                "olonglonglonglong/file.txt");
        QTest::newRow("long emoji") << QString::fromUtf8("/alonglonglonglong/blonglonglonglong/clonglonglonglong/dlonglonglonglong/"
                                                         "elonglonglonglong/flonglonglonglong/glonglonglonglong/hlonglonglonglong/ilonglonglonglong/"
                                                         "jlonglonglonglong/klonglonglonglong/llonglonglonglong/mlonglonglonglong/nlonglonglonglong/"
                                                         "olonglonglonglong/file🐷.txt");
        QTest::newRow("long russian") << QString::fromUtf8("/alonglonglonglong/blonglonglonglong/clonglonglonglong/dlonglonglonglong/"
                                                           "elonglonglonglong/flonglonglonglong/glonglonglonglong/hlonglonglonglong/ilonglonglonglong/"
                                                           "jlonglonglonglong/klonglonglonglong/llonglonglonglong/mlonglonglonglong/nlonglonglonglong/"
                                                           "olonglonglonglong/собственное.txt");
        QTest::newRow("long arabic") << QString::fromUtf8("/alonglonglonglong/blonglonglonglong/clonglonglonglong/dlonglonglonglong/"
                                                          "elonglonglonglong/flonglonglonglong/glonglonglonglong/hlonglonglonglong/ilonglonglonglong/"
                                                          "jlonglonglonglong/klonglonglonglong/llonglonglonglong/mlonglonglonglong/nlonglonglonglong/"
                                                          "olonglonglonglong/السحاب.txt");
        QTest::newRow("long chinese") << QString::fromUtf8("/alonglonglonglong/blonglonglonglong/clonglonglonglong/dlonglonglonglong/"
                                                           "elonglonglonglong/flonglonglonglong/glonglonglonglong/hlonglonglonglong/ilonglonglonglong/"
                                                           "jlonglonglonglong/klonglonglonglong/llonglonglonglong/mlonglonglonglong/nlonglonglonglong/"
                                                           "olonglonglonglong/自己的云.txt");
    }

    void testLongPathStat()
    {
        QTemporaryDir tmp;
        QFETCH(QString, name);
        const QFileInfo longPath(tmp.path() + name);

        const auto data = QByteArrayLiteral("hello");
        qDebug() << longPath;
        QVERIFY(longPath.dir().mkpath("."));

        QFile file(longPath.filePath());
        QVERIFY(file.open(QFile::WriteOnly));
        QVERIFY(file.write(data.constData()) == data.size());
        file.close();

        csync_file_stat_t buf;
        QVERIFY(csync_vio_local_stat(longPath.filePath(), &buf) != -1);
        QVERIFY(buf.size == data.size());
        QVERIFY(buf.size == longPath.size());

        QVERIFY(tmp.remove());
    }
};

QTEST_GUILESS_MAIN(TestLongWindowsPath)
#include "testlongpath.moc"
