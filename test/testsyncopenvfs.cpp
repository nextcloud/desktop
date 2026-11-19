/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "common/vfs.h"
#include "hydrationjob.h"
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

bool setPlaceholderState(const QString &absolutePath, ::OpenVFS::Constants::States state)
{
    auto attributes = ::OpenVFS::PlaceHolderAttributes::fromAttributes(FileSystem::toFilesystemPath(absolutePath));
    if (!attributes) {
        return false;
    }
    attributes->state = state;
    const auto data = attributes->toData();
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

    // A/a1 and A/a2 as dehydrated placeholders.
    static void prepareDehydratedFolder(FakeFolder &fakeFolder, Vfs &vfs)
    {
        prepareHydratedFolder(fakeFolder, vfs);
        QVERIFY(vfs.setPinState(u"A"_s, PinState::OnlineOnly));
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(dbRecord(fakeFolder, u"A/a2"_s)._type, ItemTypeVirtualFile);
        QCOMPARE(placeholderAttributes(fakeFolder, u"A/a2"_s)->state, ::OpenVFS::Constants::States::DeHydrated);
    }

    static void verifyDehydrated(FakeFolder &fakeFolder, const QString &path)
    {
        const auto record = dbRecord(fakeFolder, path);
        QCOMPARE(record._type, ItemTypeVirtualFile);
        const auto attributes = placeholderAttributes(fakeFolder, path);
        QVERIFY(attributes);
        QCOMPARE(attributes->state, ::OpenVFS::Constants::States::DeHydrated);
        QCOMPARE(attributes->size, static_cast<std::size_t>(record._fileSize));
        QCOMPARE(QFileInfo(fakeFolder.localPath() + path).size(), 0);
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

    void testFolderWithNewLocalFileCanBeDehydratedAgain()
    {
        FakeFolder fakeFolder{FileInfo{}};
        auto vfs = setupVfs(fakeFolder);
        prepareHydratedFolder(fakeFolder, *vfs);
        QVERIFY(vfs->setPinState(u"A"_s, PinState::OnlineOnly));
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(*vfs->availability(u"A"_s, Vfs::AvailabilityRecursivity::RecursiveAvailability), VfsItemAvailability::OnlineOnly);

        fakeFolder.localModifier().insert(u"A/new"_s, 32);
        QVERIFY(writeDefaultAttributes(fakeFolder.localPath() + u"A/new"_s));
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(dbRecord(fakeFolder, u"A/new"_s)._type, ItemTypeFile);
        QCOMPARE(*vfs->availability(u"A"_s, Vfs::AvailabilityRecursivity::RecursiveAvailability), VfsItemAvailability::Mixed);

        QVERIFY(vfs->setPinState(u"A"_s, PinState::OnlineOnly));
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(dbRecord(fakeFolder, u"A/new"_s)._type, ItemTypeVirtualFile);
        QCOMPARE(*vfs->availability(u"A"_s, Vfs::AvailabilityRecursivity::RecursiveAvailability), VfsItemAvailability::OnlineOnly);
    }

    void testAvailabilityOfMissingItem()
    {
        FakeFolder fakeFolder{FileInfo{}};
        auto vfs = setupVfs(fakeFolder);
        prepareHydratedFolder(fakeFolder, *vfs);

        const auto availability = vfs->availability(u"missing"_s, Vfs::AvailabilityRecursivity::RecursiveAvailability);
        QVERIFY(!availability);
        QCOMPARE(availability.error(), Vfs::AvailabilityError::NoSuchItem);
    }

    void testAvailabilityOfEmptyFolder_data()
    {
        QTest::addColumn<PinState>("pinState");
        QTest::addColumn<VfsItemAvailability>("expected");
        QTest::newRow("always local") << PinState::AlwaysLocal << VfsItemAvailability::AlwaysLocal;
        QTest::newRow("online only") << PinState::OnlineOnly << VfsItemAvailability::OnlineOnly;
        QTest::newRow("unspecified") << PinState::Unspecified << VfsItemAvailability::AllHydrated;
    }

    void testAvailabilityOfEmptyFolder()
    {
        QFETCH(PinState, pinState);
        QFETCH(VfsItemAvailability, expected);

        FakeFolder fakeFolder{FileInfo{}};
        auto vfs = setupVfs(fakeFolder);
        prepareHydratedFolder(fakeFolder, *vfs);
        fakeFolder.remoteModifier().mkdir(u"Empty"_s);
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(writeDefaultAttributes(fakeFolder.localPath() + u"Empty"_s));
        QVERIFY(vfs->setPinState(u"Empty"_s, pinState));

        const auto availability = vfs->availability(u"Empty"_s, Vfs::AvailabilityRecursivity::RecursiveAvailability);
        QVERIFY(availability);
        QCOMPARE(*availability, expected);
    }

    void testExcludingAFolderLeavesItsContent()
    {
        FakeFolder fakeFolder{FileInfo{}};
        auto vfs = setupVfs(fakeFolder);
        prepareHydratedFolder(fakeFolder, *vfs);
        QVERIFY(vfs->setPinState(u"A/a2"_s, PinState::OnlineOnly));

        // the status tracker excludes ignored folders on every sync, this must not walk their content
        QVERIFY(vfs->setPinState(u"A"_s, PinState::Excluded));

        QCOMPARE(*vfs->pinState(u"A"_s), PinState::Excluded);
        QCOMPARE(*vfs->pinState(u"A/a1"_s), PinState::Unspecified);
        QCOMPARE(*vfs->pinState(u"A/a2"_s), PinState::OnlineOnly);
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

    void testFailedHydrationRestoresThePlaceholder()
    {
        FakeFolder fakeFolder{FileInfo{}};
        auto vfs = setupVfs(fakeFolder);
        prepareDehydratedFolder(fakeFolder, *vfs);
        const auto record = dbRecord(fakeFolder, u"A/a2"_s);
        fakeFolder.setServerOverride([](QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *) -> QNetworkReply * {
            if (op == QNetworkAccessManager::GetOperation && request.url().path().endsWith(u"A/a2"_s)) {
                return new FakeErrorReply(op, request, nullptr, 500);
            }
            return nullptr;
        });

        auto *hydration = vfs->hydrateFile(record._fileId, fakeFolder.localPath() + u"A/a2"_s);
        QVERIFY(hydration);
        QSignalSpy errorSpy(hydration, &HydrationJob::error);
        hydration->start();
        QVERIFY(errorSpy.wait());

        verifyDehydrated(fakeFolder, u"A/a2"_s);
        QCOMPARE(QFileInfo(fakeFolder.localPath() + u"A/a2"_s).lastModified().toSecsSinceEpoch(), record._modtime);

        // the next access can download the file again
        fakeFolder.setServerOverride({});
        auto *retry = vfs->hydrateFile(record._fileId, fakeFolder.localPath() + u"A/a2"_s);
        QVERIFY(retry);
        QSignalSpy finishedSpy(retry, &HydrationJob::finished);
        retry->start();
        QVERIFY(finishedSpy.wait());
        QCOMPARE(placeholderAttributes(fakeFolder, u"A/a2"_s)->state, ::OpenVFS::Constants::States::Hydrated);
        QCOMPARE(QFileInfo(fakeFolder.localPath() + u"A/a2"_s).size(), 64);
    }

    void testInterruptedHydrationIsRestoredAndNotUploaded()
    {
        FakeFolder fakeFolder{FileInfo{}};
        auto vfs = setupVfs(fakeFolder);
        prepareDehydratedFolder(fakeFolder, *vfs);
        const auto remoteBefore = fakeFolder.currentRemoteState();

        // the client quit while it downloaded A/a2: partial data, still hydrating, new modification time
        const auto localPath = fakeFolder.localPath() + u"A/a2"_s;
        QFile file(localPath);
        QVERIFY(file.open(QFile::WriteOnly | QFile::Truncate));
        QCOMPARE(file.write(QByteArray(16, 'x')), 16);
        file.close();
        QVERIFY(setPlaceholderState(localPath, ::OpenVFS::Constants::States::Hydrating));

        QVERIFY(fakeFolder.syncOnce());

        QCOMPARE(fakeFolder.currentRemoteState(), remoteBefore);
        verifyDehydrated(fakeFolder, u"A/a2"_s);
    }

    void testRunningHydrationIsLeftAlone()
    {
        FakeFolder fakeFolder{FileInfo{}};
        auto vfs = setupVfs(fakeFolder);
        prepareDehydratedFolder(fakeFolder, *vfs);
        const auto remoteBefore = fakeFolder.currentRemoteState();
        const auto record = dbRecord(fakeFolder, u"A/a2"_s);

        auto *hydration = vfs->hydrateFile(record._fileId, fakeFolder.localPath() + u"A/a2"_s);
        QVERIFY(hydration);
        QVERIFY(fakeFolder.syncOnce());

        QCOMPARE(fakeFolder.currentRemoteState(), remoteBefore);
        QCOMPARE(placeholderAttributes(fakeFolder, u"A/a2"_s)->state, ::OpenVFS::Constants::States::Hydrating);

        QSignalSpy finishedSpy(hydration, &HydrationJob::finished);
        hydration->start();
        QVERIFY(finishedSpy.wait());
        QCOMPARE(placeholderAttributes(fakeFolder, u"A/a2"_s)->state, ::OpenVFS::Constants::States::Hydrated);
        QCOMPARE(QFileInfo(fakeFolder.localPath() + u"A/a2"_s).size(), 64);
    }
};

QTEST_GUILESS_MAIN(TestSyncOpenVfs)
#include "testsyncopenvfs.moc"
