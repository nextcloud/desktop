/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#ifndef MIRALL_TESTFOLDERWATCHER_H
#define MIRALL_TESTFOLDERWATCHER_H

#include <QtTest>

#include "mirall/folderwatcher.h"
#include "mirall/utility.h"

using namespace Mirall;

class TestFolderWatcher : public QObject
{
    Q_OBJECT

public slots:
    void slotFolderChanged( const QString& path ) {
        if (_skipNotifications.contains(path)) {
            return;
        }
        if (_requiredNotifications.contains(path)) {
            _receivedNotifications.insert(path);
        }
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
    QSet<QString>  _requiredNotifications;
    QSet<QString>  _receivedNotifications;
    QSet<QString>  _skipNotifications;

    void processAndWait()
    {
        _loop.processEvents();
        Utility::usleep(200000);
        _loop.processEvents();
    }

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
        Utility::writeRandomFile( _root+"/a2/renamefile");
        Utility::writeRandomFile( _root+"/a1/movefile");

        _watcher = new FolderWatcher(_root);
        QObject::connect(_watcher, SIGNAL(folderChanged(QString)), this, SLOT(slotFolderChanged(QString)));
        _timer.singleShot(5000, this, SLOT(slotEnd()));
    }

    void init()
    {
        _receivedNotifications.clear();
        _requiredNotifications.clear();
        _skipNotifications.clear();
    }

    void checkNotifications()
    {
        processAndWait();
        QCOMPARE(_receivedNotifications, _requiredNotifications);
    }

    void testACreate() { // create a new file
        QString cmd;
        _requiredNotifications.insert(_root);
        cmd = QString("echo \"xyz\" > %1/foo.txt").arg(_root);
        qDebug() << "Command: " << cmd;
        system(cmd.toLocal8Bit());

        checkNotifications();
    }

    void testATouch() { // touch an existing file.
        _requiredNotifications.insert(_root+"/a1");
#ifdef Q_OS_WIN
        Utility::writeRandomFile(QString("%1/a1/random.bin").arg(_root));
#else
        QString cmd;
        cmd = QString("/usr/bin/touch %1/a1/random.bin").arg(_root);
        qDebug() << "Command: " << cmd;
        system(cmd.toLocal8Bit());
#endif

        checkNotifications();
    }

    void testCreateADir() {
        _requiredNotifications.insert(_root+"/a1/b1");
        _skipNotifications.insert(_root + "/a1/b1/new_dir");
        QDir dir;
        dir.mkdir( _root + "/a1/b1/new_dir");
        QVERIFY(QFile::exists(_root + "/a1/b1/new_dir"));

        checkNotifications();
    }

    void testRemoveADir() {
        _requiredNotifications.insert(_root+"/a1/b3");
        QDir dir;
        QVERIFY(dir.rmdir(_root+"/a1/b3/c3"));

        checkNotifications();
    }

    void testRemoveAFile() {
        _requiredNotifications.insert(_root+"/a1/b2");
        QVERIFY(QFile::exists(_root+"/a1/b2/todelete.bin"));
        QFile::remove(_root+"/a1/b2/todelete.bin");
        QVERIFY(!QFile::exists(_root+"/a1/b2/todelete.bin"));

        checkNotifications();
    }

    void testRenameAFile() {
        _requiredNotifications.insert(_root+"/a2");
        QVERIFY(QFile::exists(_root+"/a2/renamefile"));
        QFile::rename(_root+"/a2/renamefile", _root+"/a2/renamefile.renamed" );
        QVERIFY(QFile::exists(_root+"/a2/renamefile.renamed"));

        checkNotifications();
    }

    void testMoveAFile() {
        _requiredNotifications.insert(_root+"/a1");
        _requiredNotifications.insert(_root+"/a2");
        QVERIFY(QFile::exists(_root+"/a1/movefile"));
        QFile::rename(_root+"/a1/movefile", _root+"/a2/movefile.renamed" );
        QVERIFY(QFile::exists(_root+"/a2/movefile.renamed"));

        checkNotifications();
    }

    void cleanupTestCase() {
        if( _root.startsWith(QDir::tempPath() )) {
            system( QString("rm -rf %1").arg(_root).toLocal8Bit() );
        }
        delete _watcher;
    }
};

#endif
