/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#include <QtTest>

#include "csync_exclude.h"

using namespace OCC;

#define EXCLUDE_LIST_FILE SOURCEDIR "/../../sync-exclude.lst"

class TestExcludedFiles: public QObject
{
    Q_OBJECT

private slots:
    void testFun()
    {
        ExcludedFiles excluded;
        bool excludeHidden = true;
        bool keepHidden = false;

        QVERIFY(!excluded.isExcluded("/a/b", "/a", keepHidden));
        QVERIFY(!excluded.isExcluded("/a/b~", "/a", keepHidden));
        QVERIFY(!excluded.isExcluded("/a/.b", "/a", keepHidden));
        QVERIFY(excluded.isExcluded("/a/.b", "/a", excludeHidden));

        excluded.addExcludeFilePath(EXCLUDE_LIST_FILE);
        excluded.reloadExcludeFiles();

        QVERIFY(!excluded.isExcluded("/a/b", "/a", keepHidden));
        QVERIFY(excluded.isExcluded("/a/b~", "/a", keepHidden));
        QVERIFY(!excluded.isExcluded("/a/.b", "/a", keepHidden));
        QVERIFY(excluded.isExcluded("/a/.Trashes", "/a", keepHidden));
        QVERIFY(excluded.isExcluded("/a/foo_conflict-bar", "/a", keepHidden));
        QVERIFY(excluded.isExcluded("/a/foo (conflicted copy bar)", "/a", keepHidden));
        QVERIFY(excluded.isExcluded("/a/.b", "/a", excludeHidden));
    }
};

QTEST_APPLESS_MAIN(TestExcludedFiles)
#include "testexcludedfiles.moc"
