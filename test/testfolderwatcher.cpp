#include <cstdlib>
#include <cerrno>
#include <cstring>

#include <QDebug>
#include <QDir>

#include "mirall/inotify.h"
#include "mirall/folderwatcher.h"
#include "mirall/temporarydir.h"
#include "testfolderwatcher.h"

static char dir_template[] = "/tmp/miralXXXXXX";

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

    QDir subdir = QDir(tmp.path() + "/sub1/sub2");
    QVERIFY(subdir.mkpath(tmp.path() + "/sub1/sub2"));

}

QTEST_MAIN(TestFolderWatcher)
#include "testfolderwatcher.moc"
