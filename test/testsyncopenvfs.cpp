/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "common/vfs.h"
#include "syncenginetestutils.h"
#include "vfs_openvfs.h"

#include <openvfs/openvfs.h>

#include <QtTest>

#include <sys/xattr.h>

using namespace OCC;
using namespace Qt::StringLiterals;

namespace
{

// Runs the plugin on the plain local folder, the tests do not mount the FUSE file system.
class OpenVfsWithoutFuse : public OCC::OpenVFS
{
protected:
    void startImpl(const VfsSetupParams &) override
    {
    }
};

std::optional<::OpenVFS::PlaceHolderAttributes> placeholderAttributes(const FakeFolder &folder, const QString &path)
{
    return ::OpenVFS::PlaceHolderAttributes::fromAttributes(FileSystem::toFilesystemPath(folder.localPath() + path));
}

// The FUSE file system reports these attributes for items that have none.
bool writeDefaultAttributes(const QString &absolutePath)
{
    if (::OpenVFS::PlaceHolderAttributes::fromAttributes(FileSystem::toFilesystemPath(absolutePath))) {
        return true;
    }
    auto attributes = ::OpenVFS::PlaceHolderAttributes::create(FileSystem::toFilesystemPath(absolutePath), {}, {}, 0);
    attributes.state = ::OpenVFS::Constants::States::Hydrated;
    const auto data = attributes.toData();
    return ::setxattr(absolutePath.toLocal8Bit().constData(), ::OpenVFS::Constants::XAttributeNames::Data.data(), data.data(), data.size(), 0) == 0;
}

SyncJournalFileRecord dbRecord(FakeFolder &folder, const QString &path)
{
    SyncJournalFileRecord record;
    [[maybe_unused]] const auto result = folder.syncJournal().getFileRecord(path, &record);
    return record;
}

QSharedPointer<Vfs> setupVfs(FakeFolder &folder)
{
    auto vfs = QSharedPointer<Vfs>(new OpenVfsWithoutFuse);
    folder.switchToVfs(vfs);
    return vfs;
}

}

class TestSyncOpenVfs : public QObject
{
    Q_OBJECT

private:
    // Two hydrated files in folder A. A/a1 got the pin state the client gives a file that is hydrated inside a dehydrated folder.
    static void prepareHydratedFolder(FakeFolder &fakeFolder, Vfs &vfs)
    {
        QVERIFY(writeDefaultAttributes(fakeFolder.localPath()));
        QVERIFY(vfs.setPinState({}, PinState::AlwaysLocal));
        fakeFolder.remoteModifier().mkdir(u"A"_s);
        fakeFolder.remoteModifier().insert(u"A/a1"_s, 64);
        fakeFolder.remoteModifier().insert(u"A/a2"_s, 64);
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(dbRecord(fakeFolder, u"A/a1"_s)._type, ItemTypeFile);
        QCOMPARE(dbRecord(fakeFolder, u"A/a2"_s)._type, ItemTypeFile);
        for (const auto &path : {u"A"_s, u"A/a1"_s, u"A/a2"_s}) {
            QVERIFY(writeDefaultAttributes(fakeFolder.localPath() + path));
        }

        QVERIFY(vfs.setPinState({}, PinState::Unspecified));
        QVERIFY(vfs.setPinState(u"A/a1"_s, PinState::Unspecified));
    }

private Q_SLOTS:
    void testDehydratedFolderDehydratesItsFiles()
    {
        FakeFolder fakeFolder{FileInfo{}};
        auto vfs = setupVfs(fakeFolder);
        prepareHydratedFolder(fakeFolder, *vfs);

        QVERIFY(vfs->setPinState(u"A"_s, PinState::OnlineOnly));
        QCOMPARE(*vfs->pinState(u"A/a1"_s), PinState::OnlineOnly);
        QCOMPARE(*vfs->pinState(u"A/a2"_s), PinState::OnlineOnly);
        QVERIFY(fakeFolder.syncOnce());

        for (const auto &path : {u"A/a1"_s, u"A/a2"_s}) {
            QCOMPARE(dbRecord(fakeFolder, path)._type, ItemTypeVirtualFile);
            const auto attributes = placeholderAttributes(fakeFolder, path);
            QVERIFY(attributes);
            QCOMPARE(attributes->state, ::OpenVFS::Constants::States::DeHydrated);
            QCOMPARE(QFileInfo(fakeFolder.localPath() + path).size(), 0);
        }
    }

    void testAlwaysLocalFolderHydratesItsFiles()
    {
        FakeFolder fakeFolder{FileInfo{}};
        auto vfs = setupVfs(fakeFolder);
        prepareHydratedFolder(fakeFolder, *vfs);
        QVERIFY(vfs->setPinState(u"A"_s, PinState::OnlineOnly));
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(dbRecord(fakeFolder, u"A/a2"_s)._type, ItemTypeVirtualFile);

        QVERIFY(vfs->setPinState(u"A"_s, PinState::AlwaysLocal));
        QVERIFY(fakeFolder.syncOnce());

        // the download replaces the file and its attributes, the FUSE file system reports the defaults for it
        for (const auto &path : {u"A/a1"_s, u"A/a2"_s}) {
            QCOMPARE(dbRecord(fakeFolder, path)._type, ItemTypeFile);
            QCOMPARE(QFileInfo(fakeFolder.localPath() + path).size(), 64);
        }
    }

    void testPinningAFolderKeepsExcludedItems()
    {
        FakeFolder fakeFolder{FileInfo{}};
        auto vfs = setupVfs(fakeFolder);
        prepareHydratedFolder(fakeFolder, *vfs);
        QVERIFY(vfs->setPinState(u"A/a1"_s, PinState::Excluded));

        QVERIFY(vfs->setPinState(u"A"_s, PinState::OnlineOnly));

        QCOMPARE(*vfs->pinState(u"A/a1"_s), PinState::Excluded);
        QCOMPARE(*vfs->pinState(u"A/a2"_s), PinState::OnlineOnly);
    }

    void testPinningAFileLeavesItsSiblings()
    {
        FakeFolder fakeFolder{FileInfo{}};
        auto vfs = setupVfs(fakeFolder);
        prepareHydratedFolder(fakeFolder, *vfs);

        QVERIFY(vfs->setPinState(u"A/a2"_s, PinState::OnlineOnly));

        QCOMPARE(*vfs->pinState(u"A/a1"_s), PinState::Unspecified);
        QCOMPARE(*vfs->pinState(u"A/a2"_s), PinState::OnlineOnly);
    }
};

QTEST_GUILESS_MAIN(TestSyncOpenVfs)
#include "testsyncopenvfs.moc"
