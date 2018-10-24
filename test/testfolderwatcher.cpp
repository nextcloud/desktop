/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#include <QtTest>

#include "folderwatcher.h"
#include "common/utility.h"

void touch(const QString &file)
{
#ifdef Q_OS_WIN
    OCC::Utility::writeRandomFile(file);
#else
    QString cmd;
    cmd = QString("touch %1").arg(file);
    qDebug() << "Command: " << cmd;
    system(cmd.toLocal8Bit());
#endif
}

void mkdir(const QString &file)
{
#ifdef Q_OS_WIN
    QDir dir;
    dir.mkdir(file);
#else
    QString cmd = QString("mkdir %1").arg(file);
    qDebug() << "Command: " << cmd;
    system(cmd.toLocal8Bit());
#endif
}

void rmdir(const QString &file)
{
#ifdef Q_OS_WIN
    QDir dir;
    dir.rmdir(file);
#else
    QString cmd = QString("rmdir %1").arg(file);
    qDebug() << "Command: " << cmd;
    system(cmd.toLocal8Bit());
#endif
}

void rm(const QString &file)
{
#ifdef Q_OS_WIN
    QFile::remove(file);
#else
    QString cmd = QString("rm %1").arg(file);
    qDebug() << "Command: " << cmd;
    system(cmd.toLocal8Bit());
#endif
}

void mv(const QString &file1, const QString &file2)
{
#ifdef Q_OS_WIN
    QFile::rename(file1, file2);
#else
    QString cmd = QString("mv %1 %2").arg(file1, file2);
    qDebug() << "Command: " << cmd;
    system(cmd.toLocal8Bit());
#endif
}

using namespace OCC;

class TestFolderWatcher : public QObject
{
    Q_OBJECT

    QTemporaryDir _root;
    QString _rootPath;
    QScopedPointer<FolderWatcher> _watcher;
    QScopedPointer<QSignalSpy> _pathChangedSpy;

    bool waitForPathChanged(const QString &path)
    {
        QElapsedTimer t;
        t.start();
        while (t.elapsed() < 5000) {
            // Check if it was already reported as changed by the watcher
            for (int i = 0; i < _pathChangedSpy->size(); ++i) {
                const auto &args = _pathChangedSpy->at(i);
                if (args.first().toString() == path)
                    return true;
            }
            // Wait a bit and test again (don't bother checking if we timed out or not)
            _pathChangedSpy->wait(200);
        }
        return false;
    }

public:
    TestFolderWatcher() {
        qsrand(QTime::currentTime().msec());
        QDir rootDir(_root.path());
        _rootPath = rootDir.canonicalPath();
        qDebug() << "creating test directory tree in " << _rootPath;

        rootDir.mkpath("a1/b1/c1");
        rootDir.mkpath("a1/b1/c2");
        rootDir.mkpath("a1/b2/c1");
        rootDir.mkpath("a1/b3/c3");
        rootDir.mkpath("a2/b3/c3");
        Utility::writeRandomFile( _rootPath+"/a1/random.bin");
        Utility::writeRandomFile( _rootPath+"/a1/b2/todelete.bin");
        Utility::writeRandomFile( _rootPath+"/a2/renamefile");
        Utility::writeRandomFile( _rootPath+"/a1/movefile");

        _watcher.reset(new FolderWatcher);
        _watcher->init(_rootPath);
        _pathChangedSpy.reset(new QSignalSpy(_watcher.data(), SIGNAL(pathChanged(QString))));
    }

private slots:
    void init()
    {
        _pathChangedSpy->clear();
    }

    void testACreate() { // create a new file
        QString file(_rootPath + "/foo.txt");
        QString cmd;
        cmd = QString("echo \"xyz\" > %1").arg(file);
        qDebug() << "Command: " << cmd;
        system(cmd.toLocal8Bit());

        QVERIFY(waitForPathChanged(file));
    }

    void testATouch() { // touch an existing file.
        QString file(_rootPath + "/a1/random.bin");
        touch(file);
        QVERIFY(waitForPathChanged(file));
    }

    void testCreateADir() {
        QString file(_rootPath+"/a1/b1/new_dir");
        mkdir(file);
        QVERIFY(waitForPathChanged(file));
    }

    void testRemoveADir() {
        QString file(_rootPath+"/a1/b3/c3");
        rmdir(file);
        QVERIFY(waitForPathChanged(file));
    }

    void testRemoveAFile() {
        QString file(_rootPath+"/a1/b2/todelete.bin");
        QVERIFY(QFile::exists(file));
        rm(file);
        QVERIFY(!QFile::exists(file));

        QVERIFY(waitForPathChanged(file));
    }

    void testRenameAFile() {
        QString file1(_rootPath+"/a2/renamefile");
        QString file2(_rootPath+"/a2/renamefile.renamed");
        QVERIFY(QFile::exists(file1));
        mv(file1, file2);
        QVERIFY(QFile::exists(file2));

        QVERIFY(waitForPathChanged(file1));
        QVERIFY(waitForPathChanged(file2));
    }

    void testMoveAFile() {
        QString old_file(_rootPath+"/a1/movefile");
        QString new_file(_rootPath+"/a2/movefile.renamed");
        QVERIFY(QFile::exists(old_file));
        mv(old_file, new_file);
        QVERIFY(QFile::exists(new_file));

        QVERIFY(waitForPathChanged(old_file));
        QVERIFY(waitForPathChanged(new_file));
    }
};

#ifdef Q_OS_MAC
    QTEST_MAIN(TestFolderWatcher)
#else
    QTEST_GUILESS_MAIN(TestFolderWatcher)
#endif

#include "testfolderwatcher.moc"
