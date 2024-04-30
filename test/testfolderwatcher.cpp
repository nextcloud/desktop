/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#include <QtTest>

#include "folderwatcher.h"
#include "common/utility.h"
#include "logger.h"

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

#ifdef Q_OS_LINUX
#define CHECK_WATCH_COUNT(n) QCOMPARE(_watcher->testLinuxWatchCount(), (n))
#else
#define CHECK_WATCH_COUNT(n) do {} while (false)
#endif

public:
    TestFolderWatcher()
    {
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
        _pathChangedSpy.reset(new QSignalSpy(_watcher.data(), &FolderWatcher::pathChanged));
    }

    int countFolders(const QString &path)
    {
        int n = 0;
        const auto entryList = QDir(path).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const auto &sub : entryList) {
            n += 1 + countFolders(path + '/' + sub);
        }
        return n;
    }

private slots:
    void initTestCase()
    {
        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);

        QStandardPaths::setTestModeEnabled(true);
    }

    void init()
    {
        _pathChangedSpy->clear();
        CHECK_WATCH_COUNT(countFolders(_rootPath) + 1);
    }

    void cleanup()
    {
        CHECK_WATCH_COUNT(countFolders(_rootPath) + 1);
    }

    void testACreate() { // create a new file
        QString file(_rootPath + "/foo.txt");
        QString cmd;
        cmd = QString("echo \"xyz\" > \"%1\"").arg(file);
        qDebug() << "Command: " << cmd;
        system(cmd.toLocal8Bit());

        QVERIFY(waitForPathChanged(file));
    }

    void testATouch() { // touch an existing file.
        QString file(_rootPath + "/a1/random.bin");
        touch(file);
        QVERIFY(waitForPathChanged(file));
    }

    void testMove3LevelDirWithFile() {
        QString file(_rootPath + "/a0/b/c/empty.txt");
        mkdir(_rootPath + "/a0");
        mkdir(_rootPath + "/a0/b");
        mkdir(_rootPath + "/a0/b/c");
        touch(file);
        mv(_rootPath + "/a0", _rootPath + "/a");
        QVERIFY(waitForPathChanged(_rootPath + "/a/b/c/empty.txt"));
    }


    void testCreateADir() {
        QString file(_rootPath+"/a1/b1/new_dir");
        mkdir(file);
        QVERIFY(waitForPathChanged(file));

        // Notifications from that new folder arrive too
        QString file2(_rootPath + "/a1/b1/new_dir/contained");
        touch(file2);
        QVERIFY(waitForPathChanged(file2));
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

    void testRenameDirectorySameBase() {
        QString old_file(_rootPath+"/a1/b1");
        QString new_file(_rootPath+"/a1/brename");
        QVERIFY(QFile::exists(old_file));
        mv(old_file, new_file);
        QVERIFY(QFile::exists(new_file));

        QVERIFY(waitForPathChanged(old_file));
        QVERIFY(waitForPathChanged(new_file));

        // Verify that further notifications end up with the correct paths

        QString file(_rootPath+"/a1/brename/c1/random.bin");
        touch(file);
        QVERIFY(waitForPathChanged(file));

        QString dir(_rootPath+"/a1/brename/newfolder");
        mkdir(dir);
        QVERIFY(waitForPathChanged(dir));
    }

    void testRenameDirectoryDifferentBase() {
        QString old_file(_rootPath+"/a1/brename");
        QString new_file(_rootPath+"/bren");
        QVERIFY(QFile::exists(old_file));
        mv(old_file, new_file);
        QVERIFY(QFile::exists(new_file));

        QVERIFY(waitForPathChanged(old_file));
        QVERIFY(waitForPathChanged(new_file));

        // Verify that further notifications end up with the correct paths

        QString file(_rootPath+"/bren/c1/random.bin");
        touch(file);
        QVERIFY(waitForPathChanged(file));

        QString dir(_rootPath+"/bren/newfolder2");
        mkdir(dir);
        QVERIFY(waitForPathChanged(dir));
    }

    void testDetectLockFiles()
    {
        QStringList listOfOfficeFiles = {QString(_rootPath + "/document.docx"), QString(_rootPath + "/document.odt")};
        std::sort(std::begin(listOfOfficeFiles), std::end(listOfOfficeFiles));

        const QStringList listOfOfficeLockFiles = {QString(_rootPath + "/.~lock.document.docx#"), QString(_rootPath + "/.~lock.document.odt#")};

        _watcher->setShouldWatchForFileUnlocking(true);
        
        // verify that office files for locking got detected by the watcher
        QScopedPointer<QSignalSpy> locksImposedSpy(new QSignalSpy(_watcher.data(), &FolderWatcher::filesLockImposed));

        for (const auto &officeFile : listOfOfficeFiles) {
            touch(officeFile);
            QVERIFY(waitForPathChanged(officeFile));
        }
        for (const auto &officeLockFile : listOfOfficeLockFiles) {
            touch(officeLockFile);
            QVERIFY(waitForPathChanged(officeLockFile));
        }

        locksImposedSpy->wait(_watcher->lockChangeDebouncingTimout() + 100);
        QCOMPARE(locksImposedSpy->count(), 1);
        auto lockedOfficeFilesByWatcher = locksImposedSpy->takeFirst().at(0).value<QSet<QString>>().values();
        std::sort(std::begin(lockedOfficeFilesByWatcher), std::end(lockedOfficeFilesByWatcher));
        QCOMPARE(listOfOfficeFiles.size(), lockedOfficeFilesByWatcher.size());

        for (int i = 0; i < listOfOfficeFiles.size(); ++i) {
            QVERIFY(listOfOfficeFiles.at(i) == lockedOfficeFilesByWatcher.at(i));
        }

        // verify that office files for unlocking got detected by the watcher
        QScopedPointer<QSignalSpy> locksReleasedSpy(new QSignalSpy(_watcher.data(), &FolderWatcher::filesLockReleased));

        for (const auto &officeLockFile : listOfOfficeLockFiles) {
            rm(officeLockFile);
            QVERIFY(waitForPathChanged(officeLockFile));
        }

        locksReleasedSpy->wait(_watcher->lockChangeDebouncingTimout() + 100);
        QCOMPARE(locksReleasedSpy->count(), 1);
        auto unLockedOfficeFilesByWatcher = locksReleasedSpy->takeFirst().at(0).value<QSet<QString>>().values();
        std::sort(std::begin(unLockedOfficeFilesByWatcher), std::end(unLockedOfficeFilesByWatcher));
        QCOMPARE(listOfOfficeFiles.size(), unLockedOfficeFilesByWatcher.size());

        for (int i = 0; i < listOfOfficeFiles.size(); ++i) {
            QVERIFY(listOfOfficeFiles.at(i) == unLockedOfficeFilesByWatcher.at(i));
        }

        // cleanup
        for (const auto &officeLockFile : listOfOfficeLockFiles) {
            rm(officeLockFile);
        }
        for (const auto &officeFile : listOfOfficeFiles) {
            rm(officeFile);
        }
    }
    void testDetectLockFilesExternally()
    {
        QStringList listOfOfficeFiles = {QString(_rootPath + "/document.docx"), QString(_rootPath + "/document.odt")};
        std::sort(std::begin(listOfOfficeFiles), std::end(listOfOfficeFiles));

        const QStringList listOfOfficeLockFiles = {QString(_rootPath + "/.~lock.document.docx#"), QString(_rootPath + "/.~lock.document.odt#")};

        for (const auto &officeFile : listOfOfficeFiles) {
            touch(officeFile);
        }
        for (const auto &officeLockFile : listOfOfficeLockFiles) {
            touch(officeLockFile);
        }

        _watcher.reset(new FolderWatcher);
        _watcher->init(_rootPath);
        _watcher->setShouldWatchForFileUnlocking(true);
        _pathChangedSpy.reset(new QSignalSpy(_watcher.data(), &FolderWatcher::pathChanged));
        QScopedPointer<QSignalSpy> locksImposedSpy(new QSignalSpy(_watcher.data(), &FolderWatcher::filesLockImposed));
        QScopedPointer<QSignalSpy> locksReleasedSpy(new QSignalSpy(_watcher.data(), &FolderWatcher::filesLockReleased));

        for (const auto &officeLockFile : listOfOfficeLockFiles) {
            _watcher->slotLockFileDetectedExternally(officeLockFile);
        }

        // locked files detected
        locksImposedSpy->wait(_watcher->lockChangeDebouncingTimout() + 100);
        QCOMPARE(locksImposedSpy->count(), 1);
        auto lockedOfficeFilesByWatcher = locksImposedSpy->takeFirst().at(0).value<QSet<QString>>().values();
        std::sort(std::begin(lockedOfficeFilesByWatcher), std::end(lockedOfficeFilesByWatcher));
        QCOMPARE(listOfOfficeFiles.size(), lockedOfficeFilesByWatcher.size());
        for (int i = 0; i < listOfOfficeFiles.size(); ++i) {
            QVERIFY(listOfOfficeFiles.at(i) == lockedOfficeFilesByWatcher.at(i));
        }

        // unlocked files detected
        for (const auto &officeLockFile : listOfOfficeLockFiles) {
            rm(officeLockFile);
        }
        locksReleasedSpy->wait(_watcher->lockChangeDebouncingTimout() + 100);
        QCOMPARE(locksReleasedSpy->count(), 1);
        auto unLockedOfficeFilesByWatcher = locksReleasedSpy->takeFirst().at(0).value<QSet<QString>>().values();
        std::sort(std::begin(unLockedOfficeFilesByWatcher), std::end(unLockedOfficeFilesByWatcher));
        QCOMPARE(listOfOfficeFiles.size(), unLockedOfficeFilesByWatcher.size());

        for (int i = 0; i < listOfOfficeFiles.size(); ++i) {
            QVERIFY(listOfOfficeFiles.at(i) == unLockedOfficeFilesByWatcher.at(i));
        }

        // cleanup
        for (const auto &officeFile : listOfOfficeFiles) {
            rm(officeFile);
        }
        for (const auto &officeLockFile : listOfOfficeLockFiles) {
            rm(officeLockFile);
        }
    }
};

#ifdef Q_OS_MAC
    QTEST_MAIN(TestFolderWatcher)
#else
    QTEST_GUILESS_MAIN(TestFolderWatcher)
#endif

#include "testfolderwatcher.moc"
