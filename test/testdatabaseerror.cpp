/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#include <QtTest>
#include "testutils/syncenginetestutils.h"
#include <syncengine.h>

using namespace OCC;


class TestDatabaseError : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase_data()
    {
        QTest::addColumn<Vfs::Mode>("vfsMode");
        QTest::addColumn<bool>("filesAreDehydrated");

        QTest::newRow("Vfs::Off") << Vfs::Off << false;

        if (VfsPluginManager::instance().isVfsPluginAvailable(Vfs::WindowsCfApi)) {
            QTest::newRow("Vfs::WindowsCfApi dehydrated") << Vfs::WindowsCfApi << true;

            // TODO: the hydrated version will fail due to an issue in the winvfs plugin, so leave it disabled for now.
            // QTest::newRow("Vfs::WindowsCfApi hydrated") << Vfs::WindowsCfApi << false;
        } else if (Utility::isWindows()) {
            qWarning("Skipping Vfs::WindowsCfApi");
        }
    }

    void testDatabaseError() {
        /* This test will make many iteration, at each iteration, the iᵗʰ database access will fail.
         * The test ensure that if there is a failure, the next sync recovers. And if there was
         * no error, then everything was sync'ed properly.
         */

        QFETCH_GLOBAL(Vfs::Mode, vfsMode);
        QFETCH_GLOBAL(bool, filesAreDehydrated);

        if (vfsMode == Vfs::WindowsCfApi) {
            QSKIP("Known to be broken, see https://github.com/owncloud/client-desktop-vfs-win/issues/22");
        }

        FileInfo finalState;

        auto checkAgainstFinalState = [&finalState](const FileInfo &currentRemoteState) {
            // The final state should be the same for every iteration, EXCEPT for the lastModified times:
            // these will differ (well, increase), because files are re-created with every iteration.
            return currentRemoteState.equals(finalState, FileInfo::IgnoreLastModified);
        };

        for (int count = 0; true; ++count) {
            qInfo() << "Starting Iteration" << count;

            FakeFolder fakeFolder(FileInfo::A12_B12_C12_S12(), vfsMode, filesAreDehydrated);

            // Do a couple of changes
            fakeFolder.remoteModifier().insert(QStringLiteral("A/a0"));
            fakeFolder.remoteModifier().appendByte(QStringLiteral("A/a1"));
            fakeFolder.remoteModifier().remove(QStringLiteral("A/a2"));
            fakeFolder.remoteModifier().rename(QStringLiteral("S/s1"), QStringLiteral("S/s1_renamed"));
            fakeFolder.remoteModifier().mkdir(QStringLiteral("D"));
            fakeFolder.remoteModifier().mkdir(QStringLiteral("D/subdir"));
            fakeFolder.remoteModifier().insert(QStringLiteral("D/subdir/file"));
            fakeFolder.localModifier().insert(QStringLiteral("B/b0"));
            fakeFolder.localModifier().appendByte(QStringLiteral("B/b1"));
            fakeFolder.remoteModifier().remove(QStringLiteral("B/b2"));
            fakeFolder.localModifier().mkdir(QStringLiteral("NewDir"));
            fakeFolder.localModifier().rename(QStringLiteral("C"), QStringLiteral("NewDir/C"));

            // Set the counter
            fakeFolder.syncJournal().autotestFailCounter = count;

            // run the sync
            bool result = fakeFolder.applyLocalModificationsAndSync();

            qInfo() << "Result of iteration" << count << "was" << result;

            if (fakeFolder.syncJournal().autotestFailCounter >= 0) {
                // No error was thrown, we are finished
                QVERIFY(result);
                QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
                QVERIFY(checkAgainstFinalState(fakeFolder.currentRemoteState()));
                return;
            }

            if (!result) {
                fakeFolder.syncJournal().autotestFailCounter = -1;
                // esnure we actually sync and are not blocked by ignored files...
                fakeFolder.syncJournal().wipeErrorBlacklist();
                // Try again
                QVERIFY(fakeFolder.syncOnce());
            }

            QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
            if (count == 0) {
                finalState = fakeFolder.currentRemoteState();
            } else {
                QVERIFY(checkAgainstFinalState(fakeFolder.currentRemoteState()));
            }
        }
    }
};

QTEST_GUILESS_MAIN(TestDatabaseError)
#include "testdatabaseerror.moc"
