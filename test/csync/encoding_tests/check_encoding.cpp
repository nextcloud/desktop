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
#include <stdio.h>
#include "common/filesystembase.h"
#include "torture.h"

#ifdef _WIN32
#include <string.h>
#endif

#include "torture.h"

static void check_long_win_path(void **state)
{
    (void) state; /* unused */

    {
        const char *path = "C://DATA/FILES/MUSIC/MY_MUSIC.mp3"; // check a short path
        const char *exp_path = "\\\\?\\C:\\\\DATA\\FILES\\MUSIC\\MY_MUSIC.mp3";
        QByteArray new_short = OCC::FileSystem::pathtoUNC(QByteArray::fromRawData(path, strlen(path)));
        assert_string_equal(new_short, exp_path);
    }

    {
        const char *path = "\\\\foo\\bar/MY_MUSIC.mp3";
        const char *exp_path = "\\\\foo\\bar\\MY_MUSIC.mp3";
        QByteArray new_short = OCC::FileSystem::pathtoUNC(QByteArray::fromRawData(path, strlen(path)));
        assert_string_equal(new_short, exp_path);
    }

    {
        const char *path = "//foo\\bar/MY_MUSIC.mp3";
        const char *exp_path = "\\\\foo\\bar\\MY_MUSIC.mp3";
        QByteArray new_short = OCC::FileSystem::pathtoUNC(QByteArray::fromRawData(path, strlen(path)));
        assert_string_equal(new_short, exp_path);
    }

    {
        const char *path = "\\foo\\bar";
        const char *exp_path = "\\\\?\\foo\\bar";
        QByteArray new_short = OCC::FileSystem::pathtoUNC(QByteArray::fromRawData(path, strlen(path)));
        assert_string_equal(new_short, exp_path);
    }

    {
        const char *path = "/foo/bar";
        const char *exp_path = "\\\\?\\foo\\bar";
        QByteArray new_short = OCC::FileSystem::pathtoUNC(QByteArray::fromRawData(path, strlen(path)));
        assert_string_equal(new_short, exp_path);
    }

    const char *longPath = "D://alonglonglonglong/blonglonglonglong/clonglonglonglong/dlonglonglonglong/"
            "elonglonglonglong/flonglonglonglong/glonglonglonglong/hlonglonglonglong/ilonglonglonglong/"
            "jlonglonglonglong/klonglonglonglong/llonglonglonglong/mlonglonglonglong/nlonglonglonglong/"
            "olonglonglonglong/file.txt";
    const char *longPathConv = "\\\\?\\D:\\\\alonglonglonglong\\blonglonglonglong\\clonglonglonglong\\dlonglonglonglong\\"
            "elonglonglonglong\\flonglonglonglong\\glonglonglonglong\\hlonglonglonglong\\ilonglonglonglong\\"
            "jlonglonglonglong\\klonglonglonglong\\llonglonglonglong\\mlonglonglonglong\\nlonglonglonglong\\"
            "olonglonglonglong\\file.txt";

    QByteArray new_long = OCC::FileSystem::pathtoUNC(QByteArray::fromRawData(longPath, strlen(longPath)));
    // printf( "XXXXXXXXXXXX %s %d\n", new_long, mem_reserved);

    assert_string_equal(new_long, longPathConv);

    // printf( "YYYYYYYYYYYY %ld\n", strlen(new_long));
    assert_int_equal( strlen(new_long), 286);
}

int torture_run_tests(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(check_long_win_path),

    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}

