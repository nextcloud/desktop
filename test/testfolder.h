/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#ifndef MIRALL_TESTFOLDER_H
#define MIRALL_TESTFOLDER_H

#include <QtTest>

#include "utility.h"
#include "folder.h"

using namespace OCC;

class TestFolder: public QObject
{
    Q_OBJECT
private slots:
    void testFolder()
    {
        QFETCH(QString, folder);
        QFETCH(QString, expectedFolder);
        Folder *f = new Folder("alias", folder, "http://foo.bar.net");
        QCOMPARE(f->path(), expectedFolder);
        delete f;
    }

    void testFolder_data()
    {
        QTest::addColumn<QString>("folder");
        QTest::addColumn<QString>("expectedFolder");

        QTest::newRow("unixcase") << "/foo/bar" << "/foo/bar";
        QTest::newRow("doubleslash") << "/foo//bar" << "/foo/bar";
        QTest::newRow("tripleslash") << "/foo///bar" << "/foo/bar";
        QTest::newRow("mixedslash") << "/foo/\\bar" << "/foo/bar";
        QTest::newRow("windowsfwslash") << "C:/foo/bar" << "C:/foo/bar";
        QTest::newRow("windowsbwslash") << "C:\\foo" << "C:/foo";
        QTest::newRow("windowsbwslash2") << "C:\\foo\\bar" << "C:/foo/bar";
    }

};

#endif
