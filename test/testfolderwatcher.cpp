#include <cstdlib>
#include <cerrno>
#include <cstring>

#include <QDebug>
#include <QDir>

#include "mirall/inotify.h"
#include "mirall/folderwatcher.h"
#include "mirall/temporarydir.h"
#include "testfolderwatcher.h"

void TestFolderWatcher::initTestCase()
{
    Mirall::INotify::initialize();
}

void TestFolderWatcher::cleanupTestCase()
{
    Mirall::INotify::cleanup();
}

void TestFolderWatcher::testFilesAdded()
{
    Mirall::TemporaryDir tmp;
    Mirall::FolderWatcher watcher(tmp.path());

    qDebug() << "Monitored: " << watcher.folders();

    QDir subdir = QDir(tmp.path());
    QSignalSpy spy(&watcher, SIGNAL(folderChanged(const QString &)));

    QVERIFY(subdir.mkpath(tmp.path() + "/sub1/sub2"));
    QVERIFY(subdir.mkpath(tmp.path() + "/sub2"));

     while (spy.count() == 0)
         QTest::qWait(200);

    // 2 directory changes
    QCOMPARE(spy.count(), 2);

    qDebug() << "Monitored: " << watcher.folders();
}

QTEST_MAIN(TestFolderWatcher)
#include "testfolderwatcher.moc"
