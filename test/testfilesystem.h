/*
   This software is in the public domain, furnished "as is", without technical
   support, and with no warranty, express or implied, as to its usefulness for
   any purpose.
*/

#ifndef MIRALL_TESTFILESYSTEM_H
#define MIRALL_TESTFILESYSTEM_H

#include <QtTest>
#include <QDebug>

#include "filesystem.h"
#include "utility.h"

using namespace OCC::Utility;
using namespace OCC::FileSystem;

class TestFileSystem : public QObject
{
    Q_OBJECT

    QString _root;


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
    void initTestCase() {
        qsrand(QTime::currentTime().msec());

        QString subdir("test_"+QString::number(qrand()));
        _root = QDir::tempPath() + "/" + subdir;

        QDir dir("/tmp");
        dir.mkdir(subdir);
        qDebug() << "creating test directory " << _root;
    }

    void cleanupTestCase()
    {
      if( !_root.isEmpty() )
        system(QString("rm -rf "+_root).toUtf8());
    }

    void testMd5Calc()
    {
       QString file( _root+"/file_a.bin");
       writeRandomFile(file);
       QFileInfo fi(file);
       QVERIFY(fi.exists());
       QByteArray sum = calcMd5(file);

       QByteArray sSum = shellSum("/usr/bin/md5sum", file);
       qDebug() << "calulated" << sum << "versus md5sum:"<< sSum;
       QVERIFY(!sSum.isEmpty());
       QVERIFY(!sum.isEmpty());
       QVERIFY(sSum == sum );
    }

    void testSha1Calc()
    {
       QString file( _root+"/file_b.bin");
       writeRandomFile(file);
       QFileInfo fi(file);
       QVERIFY(fi.exists());
       QByteArray sum = calcSha1(file);

       QByteArray sSum = shellSum("/usr/bin/sha1sum", file);
       qDebug() << "calulated" << sum << "versus sha1sum:"<< sSum;
       QVERIFY(!sSum.isEmpty());
       QVERIFY(!sum.isEmpty());
       QVERIFY(sSum == sum );
    }

};

#endif
