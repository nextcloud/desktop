#include <cstdlib>
#include <cerrno>
#include <cstring>

#include <QDebug>
#include <QDir>
#include <QFileInfo>

#include "mirall/inotify.h"
#include "mirall/unisonfolder.h"
#include "mirall/temporarydir.h"
#include "testunisonfolder.h"

//static char dir_template[] = "/tmp/miralXXXXXX";

void TestUnisonFolder::initTestCase()
{
    Mirall::INotify::initialize();
}

void TestUnisonFolder::cleanupTestCase()
{
    Mirall::INotify::cleanup();
}

void TestUnisonFolder::testSyncFiles()
{
    Mirall::TemporaryDir tmp1;
    Mirall::TemporaryDir tmp2;

    Mirall::UnisonFolder folder(tmp1.path(), tmp2.path(), this);

    // create a directory in the first
    QDir(tmp1.path()).mkdir("foo");
    QTest::qWait(10000);
    QVERIFY(QDir(tmp2.path() + "/foo").exists());
}

QTEST_MAIN(TestUnisonFolder)
#include "testunisonfolder.moc"
