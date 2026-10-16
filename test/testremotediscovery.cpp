/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2018 ownCloud, Inc.
 * SPDX-License-Identifier: CC0-1.0
 *
 * This software is in the public domain, furnished "as is", without technical
 * support, and with no warranty, express or implied, as to its usefulness for
 * any purpose.
 */

#include "configfile.h"
#include "syncenginetestutils.h"
#include <QScopedValueRollback>
#include <QtTest>
#include <localdiscoverytracker.h>
#include <syncengine.h>

using namespace OCC;
using namespace Qt::StringLiterals;

struct FakeBrokenXmlPropfindReply : FakePropfindReply {
    FakeBrokenXmlPropfindReply(FileInfo &remoteRootFileInfo, QNetworkAccessManager::Operation op,
                               const QNetworkRequest &request, QObject *parent)
        : FakePropfindReply(remoteRootFileInfo, op, request, parent) {
        QVERIFY(payload.size() > 50);
        // turncate the XML
        payload.chop(20);
    }
};

struct MissingPermissionsPropfindReply : FakePropfindReply {
    MissingPermissionsPropfindReply(FileInfo &remoteRootFileInfo, QNetworkAccessManager::Operation op,
                               const QNetworkRequest &request, QObject *parent)
        : FakePropfindReply(remoteRootFileInfo, op, request, parent) {
        // If the propfind contains a single file without permissions, this is a server error
        const char toRemove[] = "<oc:permissions>GRDNVCKW</oc:permissions>";
        auto pos = payload.indexOf(toRemove, payload.size()/2);
        QVERIFY(pos > 0);
        payload.remove(pos, sizeof(toRemove) - 1);
    }
};

// Reply that queues its own deletion so it fires inside LsColJob::finished()'s processEvents().
struct FakePropfindReplyWithPendingDeletion : FakePropfindReply {
    FakePropfindReplyWithPendingDeletion(FileInfo &remoteRootFileInfo, QNetworkAccessManager::Operation op,
                                         const QNetworkRequest &request, QObject *parent)
        : FakePropfindReply(remoteRootFileInfo, op, request, parent)
    {
        // respond() is queued before this timer, so it fires first; the deletion fires
        // inside LsColJob::finished()'s processEvents() call.
        QTimer::singleShot(0, this, &QObject::deleteLater);
    }
};


enum ErrorKind : int {
    // Lower code are corresponding to HTML error code
    InvalidXML = 1000,
    Timeout,
};

Q_DECLARE_METATYPE(ErrorCategory)

class TestRemoteDiscovery : public QObject
{
    Q_OBJECT

    QTemporaryDir _configDirectory;

private Q_SLOTS:
    void initTestCase()
    {
        AbstractNetworkJob::enableTimeout = true;

        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);

        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(_configDirectory.isValid());
        QVERIFY(ConfigFile::setConfDir(_configDirectory.path()));
    }

    void testLargeDirectoryYields_data()
    {
        QTest::addColumn<int>("fileCount");
        QTest::addColumn<bool>("virtualFiles");
        QTest::newRow("empty") << 0 << false;
        QTest::newRow("single") << 1 << false;
        QTest::newRow("large") << 512 << false;
        QTest::newRow("large-virtual") << 512 << true;
    }

    void testLargeDirectoryYields()
    {
        QFETCH(int, fileCount);
        QFETCH(bool, virtualFiles);
        FakeFolder folder{FileInfo::A12_B12_C12_S12()};
        if (virtualFiles) {
            auto vfs = QSharedPointer<Vfs>(createVfsFromPlugin(Vfs::WithSuffix).release());
            QVERIFY(vfs);
            folder.switchToVfs(vfs);
            folder.syncJournal().internalPinStates().setForPath("", PinState::Unspecified);
        }
        folder.remoteModifier().mkdir("A/000-child");
        folder.remoteModifier().insert("A/000-child/file");
        for (auto i = 0; i < fileCount; ++i) {
            folder.remoteModifier().insert(u"A/new-%1"_s.arg(i, 4, 10, QLatin1Char('0')));
        }

        QStringList discovered;
        auto discoveredAtYield = -1;
        auto discoveredAtTimer = -1;
        auto discoveredAtPropagation = -1;
        auto discoveredBeforeChild = -1;
        auto discoveredAtHealthReply = -1;
        auto healthRequestMatches = false;
        auto healthReplyValid = false;
        folder.setServerOverride([&](QNetworkAccessManager::Operation, const QNetworkRequest &request, QIODevice *) -> QNetworkReply * {
            if (request.rawHeader("Depth") == "0") {
                healthRequestMatches = request.attribute(QNetworkRequest::CustomVerbAttribute).toString() == "PROPFIND"_L1
                    && request.priority() == QNetworkRequest::HighPriority && request.url().path().endsWith("/dav/files/admin/"_L1);
            }
            return nullptr;
        });
        connect(&folder.syncEngine(), &SyncEngine::itemDiscovered, this, [&](const SyncFileItemPtr &item) {
            if (item->_file.startsWith("A/000-child/file"_L1)) {
                discoveredBeforeChild = discovered.size();
            }
            if (!item->_file.startsWith("A/new-"_L1)) {
                return;
            }
            QCOMPARE(item->_instruction, CSYNC_INSTRUCTION_NEW);
            QCOMPARE(item->_type, virtualFiles ? ItemTypeVirtualFile : ItemTypeFile);
            discovered.append(item->_file);
            if (discovered.size() == 1) {
                QTimer::singleShot(0, &folder.syncEngine(), [&] {
                    discoveredAtYield = discovered.size();
                });
                if (fileCount > 1) {
                    QTimer::singleShot(1, Qt::PreciseTimer, &folder.syncEngine(), [&] {
                        discoveredAtTimer = discovered.size();
                    });
                    // Exercise the same root request shape as ConnectionValidator.
                    // The fake transport isolates delivery on the event loop.
                    auto *health = new PropfindJob(folder.account(), u"/"_s, &folder.syncEngine());
                    health->setProperties({"getlastmodified"_ba});
                    connect(health, &PropfindJob::result, &folder.syncEngine(), [&](const QVariantMap &properties) {
                        discoveredAtHealthReply = discovered.size();
                        healthReplyValid = properties.contains(u"getlastmodified"_s);
                    });
                    health->start();
                }
            }
        });
        connect(&folder.syncEngine(), &SyncEngine::aboutToPropagate, this, [&](SyncFileItemVector &) {
            discoveredAtPropagation = discovered.size();
        });

        QVERIFY(folder.syncOnce());
        QCOMPARE(discovered.size(), fileCount);
        QCOMPARE(discoveredAtPropagation, fileCount);
        QCOMPARE(discoveredBeforeChild, fileCount);
        if (fileCount > 1) {
            QVERIFY(discoveredAtYield > 0);
            QVERIFY2(discoveredAtYield < fileCount, "A queued callback must run before a large directory finishes reconciliation");
            QVERIFY(discoveredAtTimer > 0);
            QVERIFY2(discoveredAtTimer < fileCount, "A nonzero timer must also run between directory batches");
            QVERIFY(healthRequestMatches);
            QVERIFY(healthReplyValid);
            QVERIFY(discoveredAtHealthReply > 0);
            QVERIFY2(discoveredAtHealthReply < fileCount, "A root-health reply must be delivered before large-directory reconciliation finishes");
        }
        auto sorted = discovered;
        sorted.sort();
        QCOMPARE(discovered, sorted);
        QCOMPARE(QSet<QString>(discovered.begin(), discovered.end()).size(), fileCount);

        for (const auto &path : std::as_const(discovered)) {
            SyncJournalFileRecord record;
            QVERIFY(folder.syncJournal().getFileRecord(path, &record));
            QVERIFY(record.isValid());
            QCOMPARE(record._type, virtualFiles ? ItemTypeVirtualFile : ItemTypeFile);
            QVERIFY(QFileInfo::exists(folder.localPath() + path));
        }
    }

    void testDirectoryBatchTimerDoesNotReenter()
    {
        FakeFolder folder{FileInfo::A12_B12_C12_S12()};
        constexpr auto fileCount = 512;
        for (auto i = 0; i < fileCount; ++i) {
            folder.remoteModifier().insert(u"A/new-%1"_s.arg(i, 4, 10, QLatin1Char('0')));
        }
        QStringList discovered;
        auto inCallback = false;
        auto reentered = false;
        connect(&folder.syncEngine(), &SyncEngine::itemDiscovered, this, [&](const SyncFileItemPtr &item) {
            if (!item->_file.startsWith("A/new-"_L1)) {
                return;
            }
            reentered = reentered || inCallback;
            const QScopedValueRollback callbackGuard(inCallback, true);
            discovered.append(item->_file);
            if (discovered.size() % 64 == 0) {
                // Let nonzero timers become ready inside a discovery receiver.
                QTest::qWait(2);
            }
        });
        QVERIFY(folder.syncOnce());
        QVERIFY(!reentered);
        QCOMPARE(discovered.size(), fileCount);
        auto sorted = discovered;
        sorted.sort();
        QCOMPARE(discovered, sorted);
        QCOMPARE(QSet<QString>(discovered.begin(), discovered.end()).size(), fileCount);
        QCOMPARE(folder.currentLocalState(), folder.currentRemoteState());
    }

    void testDirectoryBatchesAcrossSiblings_data()
    {
        QTest::addColumn<int>("parallelJobs");
        QTest::newRow("serial") << 1;
        QTest::newRow("parallel") << 2;
        QTest::newRow("five-parallel") << 5;
    }

    void testDirectoryBatchesAcrossSiblings()
    {
        QFETCH(int, parallelJobs);
        FakeFolder folder{FileInfo::A12_B12_C12_S12()};
        auto options = folder.syncEngine().syncOptions();
        options._parallelNetworkJobs = parallelJobs;
        folder.syncEngine().setSyncOptions(options);
        const QStringList directories{u"A"_s, u"B"_s, u"C"_s};
        constexpr auto fileCount = 512;
        for (const auto &directory : directories) {
            for (auto i = 0; i < fileCount; ++i) {
                folder.remoteModifier().insert(directory + u"/new-%1"_s.arg(i, 4, 10, QLatin1Char('0')));
            }
        }

        QHash<QString, int> discovered;
        QSet<QString> unfinished;
        auto peakUnfinished = 0;
        connect(&folder.syncEngine(), &SyncEngine::itemDiscovered, this, [&](const SyncFileItemPtr &item) {
            if (!item->_file.contains("/new-"_L1)) {
                return;
            }
            const auto directory = item->_file.section(QLatin1Char('/'), 0, 0);
            unfinished.insert(directory);
            peakUnfinished = qMax(peakUnfinished, static_cast<int>(unfinished.size()));
            if (++discovered[directory] == fileCount) {
                unfinished.remove(directory);
            }
        });

        QVERIFY(folder.syncOnce());
        QVERIFY(peakUnfinished <= parallelJobs);
        QVERIFY(unfinished.isEmpty());
        for (const auto &directory : directories) {
            QCOMPARE(discovered.value(directory), fileCount);
        }
        QCOMPARE(folder.currentLocalState(), folder.currentRemoteState());
    }

    void testAbortBetweenDirectoryBatches_data()
    {
        QTest::addColumn<bool>("queuedAbort");
        QTest::newRow("queued") << true;
        QTest::newRow("synchronous") << false;
    }

    void testSingleItemDiscoveryAcrossBatches()
    {
        FakeFolder folder{FileInfo::A12_B12_C12_S12()};
        constexpr auto fileCount = 512;
        for (auto i = 0; i < fileCount; ++i) {
            folder.remoteModifier().insert(u"new-%1"_s.arg(i, 4, 10, QLatin1Char('0')));
        }
        const auto selected = u"new-0511"_s;
        SyncEngine::SingleItemDiscoveryOptions options;
        options.discoveryPath = u"/"_s;
        options.filePathRelative = selected;
        folder.syncEngine().setSingleItemDiscoveryOptions(options);
        QStringList discovered;
        connect(&folder.syncEngine(), &SyncEngine::itemDiscovered, this, [&](const SyncFileItemPtr &item) {
            discovered.append(item->_file);
        });

        QVERIFY(folder.syncOnce());
        QCOMPARE(discovered, QStringList{selected});
        QVERIFY(QFileInfo::exists(folder.localPath() + selected));
        QVERIFY(!QFileInfo::exists(folder.localPath() + u"new-0000"_s));
    }

    void testAbortBetweenDirectoryBatches()
    {
        QFETCH(bool, queuedAbort);
        FakeFolder folder{FileInfo::A12_B12_C12_S12()};
        const auto originalLocalState = folder.currentLocalState();
        constexpr auto fileCount = 512;
        for (auto i = 0; i < fileCount; ++i) {
            folder.remoteModifier().insert(u"A/new-%1"_s.arg(i, 4, 10, QLatin1Char('0')));
        }

        auto discovered = 0;
        auto discoveredAtAbort = 0;
        qint64 totalFilesAtAbort = -1;
        auto abortQueued = false;
        auto propagationStarted = false;
        const auto discoveryConnection = connect(&folder.syncEngine(), &SyncEngine::itemDiscovered, this, [&](const SyncFileItemPtr &item) {
            if (!item->_file.startsWith("A/new-"_L1)) {
                return;
            }
            ++discovered;
            if (!abortQueued) {
                abortQueued = true;
                const auto abort = [&] {
                    discoveredAtAbort = discovered;
                    folder.syncEngine().abort();
                    totalFilesAtAbort = folder.syncEngine().progressInfo()->totalFiles();
                };
                if (queuedAbort) {
                    QTimer::singleShot(0, &folder.syncEngine(), abort);
                } else {
                    abort();
                }
            }
        });
        const auto propagationConnection = connect(&folder.syncEngine(), &SyncEngine::aboutToPropagate, this, [&](SyncFileItemVector &) {
            propagationStarted = true;
        });

        QVERIFY(!folder.syncOnce());
        QVERIFY(discoveredAtAbort > 0);
        QVERIFY(discoveredAtAbort < fileCount);
        QCoreApplication::processEvents();
        QCOMPARE(discovered, discoveredAtAbort);
        QCOMPARE(folder.syncEngine().progressInfo()->totalFiles(), totalFilesAtAbort);
        QVERIFY(!propagationStarted);
        QCOMPARE(folder.currentLocalState(), originalLocalState);

        disconnect(discoveryConnection);
        disconnect(propagationConnection);
        QVERIFY(folder.syncOnce());
        QCOMPARE(folder.currentLocalState(), folder.currentRemoteState());
    }

    void testRemoteDiscoveryError_data()
    {
        qRegisterMetaType<ErrorCategory>();
        QTest::addColumn<int>("errorKind");
        QTest::addColumn<QString>("expectedErrorString");
        QTest::addColumn<bool>("syncSucceeds");

        const auto itemErrorMessage = "An unexpected error occurred. Please try syncing again or contact your server administrator if the issue continues.";

        QTest::newRow("400") << 400 << QStringLiteral("We couldn’t process your request. Please try syncing again later. If this keeps happening, contact your server administrator for help.") << false;
        QTest::newRow("401") << 401 << QStringLiteral("You need to sign in to continue. If you have trouble with your credentials, please reach out to your server administrator.") << false;
        QTest::newRow("403") << 403 << QStringLiteral("You don’t have access to this resource. If you think this is a mistake, please contact your server administrator.") << true;
        QTest::newRow("404") << 404 << QStringLiteral("We couldn’t find what you were looking for. It might have been moved or deleted. If you need help, contact your server administrator.") << true;
        QTest::newRow("407") << 407 << QStringLiteral("It seems you are using a proxy that required authentication. Please check your proxy settings and credentials. If you need help, contact your server administrator.") << true;
        QTest::newRow("408") << 408 << QStringLiteral("The request is taking longer than usual. Please try syncing again. If it still doesn’t work, reach out to your server administrator.") << true;
        QTest::newRow("409") << 409 << QStringLiteral("Server files changed while you were working. Please try syncing again. Contact your server administrator if the issue persists.") << true;
        QTest::newRow("410") << 410 << QStringLiteral("This folder or file isn’t available anymore. If you need assistance, please contact your server administrator.") << true;
        QTest::newRow("412") << 412 << QStringLiteral("The request could not be completed because some required conditions were not met. Please try syncing again later. If you need assistance, please contact your server administrator.") << true;
        QTest::newRow("413") << 413 << QStringLiteral("The file is too big to upload. You might need to choose a smaller file or contact your server administrator for assistance.") << true;
        QTest::newRow("414") << 414 << QStringLiteral("The address used to make the request is too long for the server to handle. Please try shortening the information you’re sending or contact your server administrator for assistance.") << true;
        QTest::newRow("415") << 415 << QStringLiteral("This file type isn’t supported. Please contact your server administrator for assistance.") << true;
        QTest::newRow("422") << 422 << QStringLiteral("The server couldn’t process your request because some information was incorrect or incomplete. Please try syncing again later, or contact your server administrator for assistance.") << true;
        QTest::newRow("423") << 423 << QStringLiteral("The resource you are trying to access is currently locked and cannot be modified. Please try changing it later, or contact your server administrator for assistance.") << true;
        QTest::newRow("428") << 428 << QStringLiteral("This request could not be completed because it is missing some required conditions. Please try again later, or contact your server administrator for help.") << true;
        QTest::newRow("429") << 429 << QStringLiteral("You made too many requests. Please wait and try again. If you keep seeing this, your server administrator can help.") << true;
        QTest::newRow("500") << 500 << QStringLiteral("Something went wrong on the server. Please try syncing again later, or contact your server administrator if the issue persists.") << true;
        QTest::newRow("502") << 502 << QStringLiteral("We’re having trouble connecting to the server. Please try again soon. If the issue persists, your server administrator can help you.") << true;
        QTest::newRow("503") << 503 << QStringLiteral("The server is busy right now. Please try connecting again in a few minutes or contact your server administrator if it’s urgent.") << true;
        QTest::newRow("504") << 504 << QStringLiteral("It’s taking too long to connect to the server. Please try again later. If you need help, contact your server administrator.") << true;
        QTest::newRow("505") << 505 << QStringLiteral("The server does not support the version of the connection being used. Contact your server administrator for help.") << true;
        QTest::newRow("507") << 507 << QStringLiteral("The server does not have enough space to complete your request. Please check how much quota your user has by contacting your server administrator.") << true;
        QTest::newRow("511") << 511 << QStringLiteral("Your network needs extra authentication. Please check your connection. Contact your server administrator for help if the issue persists.") << true;
        QTest::newRow("513") << 513 << QStringLiteral("You don’t have permission to access this resource. If you believe this is an error, contact your server administrator to ask for assistance.") << true;
        // 200 should be an error since propfind should return 207
        QTest::newRow("200") << 200 << itemErrorMessage << false;
        QTest::newRow("InvalidXML") << +InvalidXML << itemErrorMessage << false;
        QTest::newRow("Timeout") << +Timeout << QStringLiteral("The server took too long to respond. Check your connection and try syncing again. If it still doesn’t work, reach out to your server administrator.") << false;
    }


    // Check what happens when there is an error.
    void testRemoteDiscoveryError()
    {
        QFETCH(int, errorKind);
        QFETCH(QString, expectedErrorString);
        QFETCH(bool, syncSucceeds);

        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };

        // Do Some change as well
        fakeFolder.localModifier().insert("A/z1");
        fakeFolder.localModifier().insert("B/z1");
        fakeFolder.localModifier().insert("C/z1");
        fakeFolder.remoteModifier().insert("A/z2");
        fakeFolder.remoteModifier().insert("B/z2");
        fakeFolder.remoteModifier().insert("C/z2");

        auto oldLocalState = fakeFolder.currentLocalState();
        auto oldRemoteState = fakeFolder.currentRemoteState();

        QString errorFolder = "dav/files/admin/B";
        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &req, QIODevice *)
                -> QNetworkReply *{
            if (req.attribute(QNetworkRequest::CustomVerbAttribute).toString() == "PROPFIND" && req.url().path().endsWith(errorFolder)) {
                if (errorKind == InvalidXML) {
                    return new FakeBrokenXmlPropfindReply(fakeFolder.remoteModifier(), op, req, this);
                } else if (errorKind == Timeout) {
                    return new FakeHangingReply(op, req, this);
                } else if (errorKind < 1000) {
                    return new FakeErrorReply(op, req, this, errorKind);
                }
            }
            return nullptr;
        });

        // So the test that test timeout finishes fast
        QScopedValueRollback<int> setHttpTimeout(AbstractNetworkJob::httpTimeout, errorKind == Timeout ? 1 : 10000);

        ItemCompletedSpy completeSpy(fakeFolder);
        QSignalSpy errorSpy(&fakeFolder.syncEngine(), &SyncEngine::syncError);
        QCOMPARE(fakeFolder.syncOnce(), syncSucceeds);

        // The folder B should not have been sync'ed (and in particular not removed)
        QCOMPARE(oldLocalState.children["B"], fakeFolder.currentLocalState().children["B"]);
        QCOMPARE(oldRemoteState.children["B"], fakeFolder.currentRemoteState().children["B"]);
        if (!syncSucceeds) {
            QCOMPARE(errorSpy.size(), 1);
            QCOMPARE(errorSpy[0][0].toString(), expectedErrorString);
        } else {
            QCOMPARE(completeSpy.findItem("B")->_instruction, CSYNC_INSTRUCTION_IGNORE);
            QVERIFY(completeSpy.findItem("B")->_errorString.contains(expectedErrorString));

            // The other folder should have been sync'ed as the sync just ignored the faulty dir
            QCOMPARE(fakeFolder.currentRemoteState().children["A"], fakeFolder.currentLocalState().children["A"]);
            QCOMPARE(fakeFolder.currentRemoteState().children["C"], fakeFolder.currentLocalState().children["C"]);
            QCOMPARE(completeSpy.findItem("A/z1")->_instruction, CSYNC_INSTRUCTION_NEW);
        }

        //
        // Check the same discovery error on the sync root
        //
        errorFolder = "dav/files/admin/";
        errorSpy.clear();
        QVERIFY(!fakeFolder.syncOnce());
        QCOMPARE(errorSpy.size(), 1);
        QCOMPARE(errorSpy[0][0].toString(), expectedErrorString);
    }

    void testMissingData()
    {
        FakeFolder fakeFolder{ FileInfo() };
        fakeFolder.remoteModifier().insert("good");
        fakeFolder.remoteModifier().insert("noetag");
        fakeFolder.remoteModifier().find("noetag")->etag.clear();
        fakeFolder.remoteModifier().insert("nofileid");
        fakeFolder.remoteModifier().find("nofileid")->fileId.clear();
        fakeFolder.remoteModifier().mkdir("nopermissions");
        fakeFolder.remoteModifier().insert("nopermissions/A");

        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &req, QIODevice *) -> QNetworkReply * {
            if (req.attribute(QNetworkRequest::CustomVerbAttribute).toString() == "PROPFIND" && req.url().path().endsWith("nopermissions")) {
                return new MissingPermissionsPropfindReply(fakeFolder.remoteModifier(), op, req, this);
            }
            return nullptr;
        });

        ItemCompletedSpy completeSpy(fakeFolder);
        QVERIFY(!fakeFolder.syncOnce());

        QCOMPARE(completeSpy.findItem("good")->_instruction, CSYNC_INSTRUCTION_NEW);
        QCOMPARE(completeSpy.findItem("noetag")->_instruction, CSYNC_INSTRUCTION_ERROR);
        QCOMPARE(completeSpy.findItem("nofileid")->_instruction, CSYNC_INSTRUCTION_ERROR);
        QCOMPARE(completeSpy.findItem("nopermissions")->_instruction, CSYNC_INSTRUCTION_NEW);
        QCOMPARE(completeSpy.findItem("nopermissions/A")->_instruction, CSYNC_INSTRUCTION_ERROR);
        QVERIFY(completeSpy.findItem("noetag")->_errorString.contains("File is not accessible on the server"));
        QVERIFY(completeSpy.findItem("nofileid")->_errorString.contains("File is not accessible on the server"));
        QVERIFY(completeSpy.findItem("nopermissions/A")->_errorString.contains("File is not accessible on the server"));
    }

    void testQuotaReportedAsDouble()
    {
        FakeFolder fakeFolder{ FileInfo() };
        fakeFolder.remoteModifier().mkdir("doubleValue");
        fakeFolder.remoteModifier().find("doubleValue")->folderQuota.setBytesAvailableString("2.345E+12");
        fakeFolder.remoteModifier().mkdir("intValue");
        fakeFolder.remoteModifier().find("intValue")->folderQuota.setBytesAvailableString("2345000000000");
        fakeFolder.remoteModifier().mkdir("unlimited");
        fakeFolder.remoteModifier().find("unlimited")->folderQuota.setBytesAvailableString("-3");
        fakeFolder.remoteModifier().mkdir("invalidValue");
        fakeFolder.remoteModifier().find("invalidValue")->folderQuota.setBytesAvailableString("maybe like, 3 GB");

        ItemCompletedSpy completeSpy(fakeFolder);
        QVERIFY(fakeFolder.syncOnce());

        int64_t expectedValue = 2345000000000;
        QCOMPARE(completeSpy.findItem("doubleValue")->_folderQuota.bytesAvailable, expectedValue);
        QCOMPARE(completeSpy.findItem("intValue")->_folderQuota.bytesAvailable, expectedValue);
        QCOMPARE(completeSpy.findItem("unlimited")->_folderQuota.bytesAvailable, -3);
        QCOMPARE(completeSpy.findItem("invalidValue")->_folderQuota.bytesAvailable, -1);
    }

    void testRootFileIdReceived()
    {
        // disable FakeFolder's initial sync run to allow for spying on the `rootFileIdReceived` signal
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12(), {}, {}, false };

        QSignalSpy rootFileIdSpy(&fakeFolder.syncEngine(), &SyncEngine::rootFileIdReceived);
        QVERIFY(fakeFolder.syncOnce());

        // root file id signal should only be emitted once
        QCOMPARE(rootFileIdSpy.size(), 1);
        const auto expectedRootFileId = fakeFolder.currentRemoteState().numericFileId();
        const auto receivedRootFileId = QByteArray::number(rootFileIdSpy.takeFirst().at(0).toLongLong(), 10);
        QCOMPARE(receivedRootFileId, expectedRootFileId);

        // another sync should not emit the root file id signal again as it's already known
        rootFileIdSpy.clear();
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(rootFileIdSpy.size(), 0);
    }

    // Regression: processEvents() inside LsColJob::finished() must not run before reply()
    // is read. A pending deleteLater() draining during processEvents() zeros the QPointer
    // and caused a SIGSEGV at reply()->request().url().path() (address 0x8).
    void testLsColJobDoesNotCrashWhenReplyIsDeletedDuringProcessEvents()
    {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        fakeFolder.remoteModifier().mkdir("B/sub");

        const auto errorFolder = QStringLiteral("dav/files/admin/B");
        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op,
                                         const QNetworkRequest &req,
                                         QIODevice *) -> QNetworkReply * {
            if (req.attribute(QNetworkRequest::CustomVerbAttribute).toString() == QStringLiteral("PROPFIND")
                && req.url().path().endsWith(errorFolder)) {
                return new FakePropfindReplyWithPendingDeletion(fakeFolder.remoteModifier(), op, req, nullptr);
            }
            return nullptr;
        });

        // The sync must complete without crashing. Discovery of B will fail gracefully
        // because finishedWithoutError is still emitted before processEvents() runs.
        QVERIFY(fakeFolder.syncOnce());
        // Items under A and C must have synced normally.
        QVERIFY(fakeFolder.currentLocalState().find("A"));
        QVERIFY(fakeFolder.currentLocalState().find("C"));
    }

    // Verifies normal listing succeeds when the reply stays alive throughout.
    void testLsColJobSucceedsNormally()
    {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        fakeFolder.remoteModifier().insert("A/newfile.txt");

        // Normal sync with no overrides: all PROPFIND replies remain alive.
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(fakeFolder.currentLocalState().find("A/newfile.txt"));
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }
};

QTEST_GUILESS_MAIN(TestRemoteDiscovery)
#include "testremotediscovery.moc"
