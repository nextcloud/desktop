#include <cstdlib>
#include <cerrno>
#include <cstring>

#include <QDebug>
#include <QDir>

#include "mirall/inotify.h"
#include "mirall/temporarydir.h"
#include "testfolderwatcher.h"

void TestFolderWatcher::initTestCase()
{

}

void TestFolderWatcher::cleanupTestCase()
{
}

void TestFolderWatcher::testFilesAdded()
{
    Mirall::INotify::initialize();
    Mirall::TemporaryDir tmp;
    Mirall::FolderWatcher watcher(tmp.path());

    // lower the event interval
    watcher.setEventInterval(1);

    qDebug() << "Monitored: " << watcher.folders();

    QDir subdir = QDir(tmp.path());
    QSignalSpy spy(&watcher, SIGNAL(folderChanged(const QStringList &)));

    QVERIFY(subdir.mkpath(tmp.path() + "/sub1/sub2"));
    QVERIFY(subdir.mkpath(tmp.path() + "/sub2"));

     while (spy.count() == 0)
         QTest::qWait(1010);

    // 1 directory changes
    QCOMPARE(spy.count(), 1);
    QList<QVariant> arguments = spy.takeFirst();
    QStringList paths = arguments.at(0).toStringList();
    qDebug() << paths;
    QCOMPARE(paths.size(), 2);

    qDebug() << "Monitored: " << watcher.folders();

    // the new sub2 directory should be now also bee in the list of watches
    QFile file(tmp.path() + "/sub1/sub2/foo.txt");
    file.open(QIODevice::WriteOnly);
    file.write("hello", 5);
    file.close();

    //while (spy.count() == )
    QTest::qWait(1010);

    // 1 file changes
    QCOMPARE(spy.count(), 1);


    Mirall::INotify::cleanup();
}

QTEST_MAIN(TestFolderWatcher)
#include "testfolderwatcher.moc"
