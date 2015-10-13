/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#pragma once

#include <QtTest>

#include "excludedfiles.h"

using namespace OCC;

#define STR_(X) #X
#define STR(X) STR_(X)
#define BIN_PATH STR(OWNCLOUD_BIN_PATH)

class TestExcludedFiles: public QObject
{
    Q_OBJECT

private slots:
    void testFun()
    {
        auto & excluded = ExcludedFiles::instance();
        bool excludeHidden = true;
        bool keepHidden = false;

        QVERIFY(!excluded.isExcluded("/a/b", "b", keepHidden));
        QVERIFY(!excluded.isExcluded("/a/b~", "b~", keepHidden));
        QVERIFY(!excluded.isExcluded("/a/.b", ".b", keepHidden));
        QVERIFY(excluded.isExcluded("/a/.b", ".b", excludeHidden));

        QString path(BIN_PATH);
        path.append("/sync-exclude.lst");
        excluded.addExcludeFilePath(path);
        excluded.reloadExcludes();

        QVERIFY(!excluded.isExcluded("/a/b", "b", keepHidden));
        QVERIFY(excluded.isExcluded("/a/b~", "b~", keepHidden));
        QVERIFY(!excluded.isExcluded("/a/.b", ".b", keepHidden));
        QVERIFY(excluded.isExcluded("/a/.Trashes", ".Trashes", keepHidden));
        QVERIFY(excluded.isExcluded("/a/foo_conflict-bar", "foo_conflict-bar", keepHidden));
        QVERIFY(excluded.isExcluded("/a/.b", ".b", excludeHidden));
    }
};


