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
}

void TestUnisonFolder::cleanupTestCase()
{
}

void TestUnisonFolder::testSyncFiles()
{
    Mirall::TemporaryDir tmp1;
    Mirall::TemporaryDir tmp2;

    qDebug() << tmp1.path() << tmp2.path();

    Mirall::INotify::initialize();
    Mirall::UnisonFolder folder("alias", tmp1.path(), tmp2.path(), this);

    // create a directory in the first
    QDir(tmp1.path()).mkdir("foo");
    QTest::qWait(1000);
    QVERIFY(QDir(tmp2.path() + "/foo").exists());

    Mirall::INotify::cleanup();
}

QTEST_MAIN(TestUnisonFolder)
#include "testunisonfolder.moc"
