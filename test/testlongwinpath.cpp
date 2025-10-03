/*
 * libcsync -- a library to sync a directory with another
 *
 * Copyright (c) 2013 by Klaas Freitag <freitag@owncloud.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
#include "common/filesystembase.h"

#include <QTest>


class TestLongWindowsPath : public QObject
{
    Q_OBJECT

private Q_SLOTS:
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
};

QTEST_GUILESS_MAIN(TestLongWindowsPath)
#include "testlongwinpath.moc"
