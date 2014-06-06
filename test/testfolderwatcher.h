/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#ifndef MIRALL_TESTFOLDERWATCHER_H
#define MIRALL_TESTFOLDERWATCHER_H

#include <QtTest>

#include "mirall/folderwatcher_linux.h"
#include "mirall/utility.h"

using namespace Mirall;

class TestFolderWatcher : public QObject
{
    Q_OBJECT

public slots:
    void slotFolderChanged( const QString& path ) {
        qDebug() << "COMPARE: " << path << _checkMark;
        QVERIFY(_checkMark == path);
        _checkMark.clear();
    }

    void slotEnd() { // in case something goes wrong...
        _loop.quit();
        QVERIFY2(1 == 0, "Loop hang!");
    }

private:
    QString        _root;
    FolderWatcher *_watcher;
    QEventLoop     _loop;
    QTimer         _timer;
    QString        _checkMark;

private slots:
    void initTestCase() {
        qsrand(QTime::currentTime().msec());
        _root = QDir::tempPath() + "/" + "test_" + QString::number(qrand());
        qDebug() << "creating test directory tree in " << _root;
        QDir rootDir(_root);

        rootDir.mkpath(_root + "/a1/b1/c1");
        rootDir.mkpath(_root + "/a1/b1/c2");
        rootDir.mkpath(_root + "/a1/b2/c1");
        rootDir.mkpath(_root + "/a1/b3/c3");
        rootDir.mkpath(_root + "/a2/b3/c3");
        Utility::writeRandomFile( _root+"/a1/random.bin");
        Utility::writeRandomFile( _root+"/a1/b2/todelete.bin");
        Utility::writeRandomFile( _root+"/a2/movefile");

        _watcher = new FolderWatcher(_root);
        QObject::connect(_watcher, SIGNAL(folderChanged(QString)), this, SLOT(slotFolderChanged(QString)));
        _timer.singleShot(3000, this, SLOT(slotEnd()));
    }

    void testACreate() { // create a new file
        QString cmd;
        _checkMark = _root;
        cmd = QString("echo \"xyz\" > %1/foo.txt").arg(_root);
        qDebug() << "Command: " << cmd;
        system(cmd.toLocal8Bit());

        _loop.processEvents();
        QVERIFY(_checkMark.isEmpty()); // the slot clears the checkmark.
    }

    void testATouch() { // touch an existing file.
        QString cmd;
        cmd = QString("/usr/bin/touch %1/a1/random.bin").arg(_root);
        _checkMark = _root+"/a1";
        qDebug() << "Command: " << cmd;
        system(cmd.toLocal8Bit());

        _loop.processEvents();
        QVERIFY(_checkMark.isEmpty()); // the slot clears the checkmark.
    }

    void testCreateADir() {
        _checkMark = _root+"/a1/b1";
        QDir dir;
        dir.mkdir( _root + "/a1/b1/new_dir");
        QVERIFY(QFile::exists(_root + "/a1/b1/new_dir"));
        _loop.processEvents();
        QVERIFY(_checkMark.isEmpty()); // the slot clears the checkmark.
    }

    void testRemoveADir() {
        _checkMark = _root+"/a1/b3";
        QDir dir;
        QVERIFY(dir.rmdir(_root+"/a1/b3/c3"));
        _loop.processEvents();
        QVERIFY(_checkMark.isEmpty()); // the slot clears the checkmark.
    }

    void testRemoveAFile() {
        _checkMark = _root+"/a1/b2";
        QVERIFY(QFile::exists(_root+"/a1/b2/todelete.bin"));
        QFile::remove(_root+"/a1/b2/todelete.bin");
        QVERIFY(!QFile::exists(_root+"/a1/b2/todelete.bin"));
        _loop.processEvents();
        QVERIFY(_checkMark.isEmpty()); // the slot clears the checkmark.
    }

    void testMoveAFile() {
        _checkMark = _root+"/a2";
        QVERIFY(QFile::exists(_root+"/a2/movefile"));
        QFile::rename(_root+"/a2/movefile", _root+"/a2/movefile.renamed" );
        QVERIFY(QFile::exists(_root+"/a2/movefile.renamed"));
        _loop.processEvents();
        QVERIFY(_checkMark.isEmpty()); // the slot clears the checkmark.
    }

    void cleanupTestCase() {
        if( _root.startsWith(QDir::tempPath() )) {
            system( QString("rm -rf %1").arg(_root).toLocal8Bit() );
        }
        delete _watcher;
    }
};

#endif
