/*
   This software is in the public domain, furnished "as is", without technical
   support, and with no warranty, express or implied, as to its usefulness for
   any purpose.
*/

#include <QtTest>
#include <QDebug>

#include "filesystem.h"
#include "common/utility.h"

using namespace OCC::Utility;
using namespace OCC::FileSystem;

class TestFileSystem : public QObject
{
    Q_OBJECT

    QTemporaryDir _root;


    QByteArray shellSum( const QByteArray& cmd, const QString& file )
    {
       QProcess md5;
       QStringList args;
       args.append(file);
       md5.start(cmd, args);
       QByteArray sumShell;
       qDebug() << "File: "<< file;

       if( md5.waitForFinished()  ) {

         sumShell = md5.readAll();
         sumShell = sumShell.left( sumShell.indexOf(' '));
       }
       return sumShell;
    }

private slots:
    void testMd5Calc()
    {
        QString file( _root.path() + "/file_a.bin");
        QVERIFY(writeRandomFile(file));
        QFileInfo fi(file);
        QVERIFY(fi.exists());
        QByteArray sum = calcMd5(file);

        QByteArray sSum = shellSum("md5sum", file);
        if (sSum.isEmpty())
            QSKIP("Couldn't execute md5sum to calculate checksum, executable missing?", SkipSingle);

        QVERIFY(!sum.isEmpty());
        QCOMPARE(sSum, sum);
    }

    void testSha1Calc()
    {
        QString file( _root.path() + "/file_b.bin");
        writeRandomFile(file);
        QFileInfo fi(file);
        QVERIFY(fi.exists());
        QByteArray sum = calcSha1(file);

        QByteArray sSum = shellSum("sha1sum", file);
        if (sSum.isEmpty())
            QSKIP("Couldn't execute sha1sum to calculate checksum, executable missing?", SkipSingle);

        QVERIFY(!sum.isEmpty());
        QCOMPARE(sSum, sum);
    }

};

QTEST_APPLESS_MAIN(TestFileSystem)
#include "testfilesystem.moc"
