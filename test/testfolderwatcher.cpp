/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#include <QtTest>

#include "folderwatcher.h"
#include "utility.h"

using namespace OCC;

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
        QObject::connect(_watcher, SIGNAL(pathChanged(QString)), this, SLOT(slotFolderChanged(QString)));
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
        QString file(_root + "/foo.txt");
        QString cmd;
        _requiredNotifications.insert(file);
        cmd = QString("echo \"xyz\" > %1").arg(file);
        qDebug() << "Command: " << cmd;
        system(cmd.toLocal8Bit());

        checkNotifications();
    }

    void testATouch() { // touch an existing file.
        QString file(_root + "/a1/random.bin");
        _requiredNotifications.insert(file);
#ifdef Q_OS_WIN
        Utility::writeRandomFile(QString("%1/a1/random.bin").arg(_root));
#else
        QString cmd;
        cmd = QString("touch %1").arg(file);
        qDebug() << "Command: " << cmd;
        system(cmd.toLocal8Bit());
#endif

        checkNotifications();
    }

    void testCreateADir() {
        QString file(_root+"/a1/b1/new_dir");
        _requiredNotifications.insert(file);
        //_skipNotifications.insert(_root + "/a1/b1/new_dir");
        QDir dir;
        dir.mkdir(file);
        QVERIFY(QFile::exists(file));

        checkNotifications();
    }

    void testRemoveADir() {
        QString file(_root+"/a1/b3/c3");
        _requiredNotifications.insert(file);
        QDir dir;
        QVERIFY(dir.rmdir(file));

        checkNotifications();
    }

    void testRemoveAFile() {
        QString file(_root+"/a1/b2/todelete.bin");
        _requiredNotifications.insert(file);
        QVERIFY(QFile::exists(file));
        QFile::remove(file);
        QVERIFY(!QFile::exists(file));

        checkNotifications();
    }

    void testRenameAFile() {
        QString file1(_root+"/a2/renamefile");
        QString file2(_root+"/a2/renamefile.renamed");
        _requiredNotifications.insert(file1);
        _requiredNotifications.insert(file2);
        QVERIFY(QFile::exists(file1));
        QFile::rename(file1, file2);
        QVERIFY(QFile::exists(file2));

        checkNotifications();
    }

    void testMoveAFile() {
        QString old_file(_root+"/a1/movefile");
        QString new_file(_root+"/a2/movefile.renamed");
        _requiredNotifications.insert(old_file);
        _requiredNotifications.insert(new_file);
        QVERIFY(QFile::exists(old_file));
        QFile::rename(old_file, new_file);
        QVERIFY(QFile::exists(new_file));

        checkNotifications();
    }

    void cleanupTestCase() {
        if( _root.startsWith(QDir::tempPath() )) {
            system( QString("rm -rf %1").arg(_root).toLocal8Bit() );
        }
        delete _watcher;
    }
};

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
// Qt4 does not have QTEST_GUILESS_MAIN, so we simulate it.
int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    TestFolderWatcher tc;
    return QTest::qExec(&tc, argc, argv);
}
#else
    QTEST_GUILESS_MAIN(TestFolderWatcher)
#endif

#include "testfolderwatcher.moc"
