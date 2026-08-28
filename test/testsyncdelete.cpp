/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2018 ownCloud, Inc.
 * SPDX-License-Identifier: CC0-1.0
 * 
 * This software is in the public domain, furnished "as is", without technical
 * support, and with no warranty, express or implied, as to its usefulness for
 * any purpose.
 */

#include <QtTest>
#include "syncenginetestutils.h"
#include <syncengine.h>

using namespace OCC;

class TestSyncDelete : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);

        QStandardPaths::setTestModeEnabled(true);
    }

    void testDeleteDirectoryWithNewFile()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };

        // Remove a directory on the server with new files on the client
        fakeFolder.remoteModifier().remove("A");
        fakeFolder.localModifier().insert("A/hello.txt");

        // Symmetry
        fakeFolder.localModifier().remove("B");
        fakeFolder.remoteModifier().insert("B/hello.txt");

        QVERIFY(fakeFolder.syncOnce());

        // A/a1 must be gone because the directory was removed on the server, but hello.txt must be there
        QVERIFY(!fakeFolder.currentRemoteState().find("A/a1"));
        QVERIFY(!fakeFolder.currentRemoteState().find("A/hello.txt"));

        // Symmetry
        QVERIFY(!fakeFolder.currentRemoteState().find("B/b1"));
        QVERIFY(!fakeFolder.currentRemoteState().find("B/hello.txt"));

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void issue1329()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };

        fakeFolder.localModifier().remove("B");
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // Add a directory that was just removed in the previous sync:
        fakeFolder.localModifier().mkdir("B");
        fakeFolder.localModifier().insert("B/b1");
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(fakeFolder.currentRemoteState().find("B/b1"));
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void remoteDeletionProtectionRestoresMissingLocalFile()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };
        auto deleteRequests = 0;
        fakeFolder.setServerOverride([&deleteRequests](QNetworkAccessManager::Operation operation,
                                                        const QNetworkRequest &request,
                                                        QIODevice *) -> QNetworkReply * {
            if (operation == QNetworkAccessManager::DeleteOperation
                || request.attribute(QNetworkRequest::CustomVerbAttribute).toString() == QStringLiteral("DELETE")) {
                ++deleteRequests;
            }
            return nullptr;
        });

        QVERIFY(fakeFolder.syncJournal().armRemoteDeletionProtection({ QStringLiteral("A/a1") }));
        fakeFolder.localModifier().remove(QStringLiteral("A/a1"));
        fakeFolder.syncJournal().close();
        QVERIFY(fakeFolder.syncJournal().open());

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(deleteRequests, 0);
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        const auto roots = fakeFolder.syncJournal().pendingRemoteDeletionProtectionRoots();
        QVERIFY(roots);
        QVERIFY(roots->isEmpty());
    }

    void remoteDeletionProtectionSurvivesPartialLocalRemoval()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };
        const auto initialRemoteState = fakeFolder.currentRemoteState();
        auto deleteRequests = 0;
        fakeFolder.setServerOverride([&deleteRequests](QNetworkAccessManager::Operation operation,
                                                        const QNetworkRequest &request,
                                                        QIODevice *) -> QNetworkReply * {
            if (operation == QNetworkAccessManager::DeleteOperation
                || request.attribute(QNetworkRequest::CustomVerbAttribute).toString() == QStringLiteral("DELETE")) {
                ++deleteRequests;
            }
            return nullptr;
        });

        QVERIFY(fakeFolder.syncJournal().armRemoteDeletionProtection({ QStringLiteral("A") }));
        fakeFolder.localModifier().remove(QStringLiteral("A/a1"));
        fakeFolder.localModifier().remove(QStringLiteral("A/a2"));
        fakeFolder.syncJournal().close();
        QVERIFY(fakeFolder.syncJournal().open());

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(deleteRequests, 0);
        QCOMPARE(fakeFolder.currentLocalState(), initialRemoteState);
        const auto roots = fakeFolder.syncJournal().pendingRemoteDeletionProtectionRoots();
        QVERIFY(roots);
        QVERIFY(roots->isEmpty());
    }

    void remoteDeletionProtectionClearsStaleMarker()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };
        QVERIFY(fakeFolder.syncJournal().armRemoteDeletionProtection({ QStringLiteral("A") }));

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        const auto roots = fakeFolder.syncJournal().pendingRemoteDeletionProtectionRoots();
        QVERIFY(roots);
        QVERIFY(roots->isEmpty());
    }

    void remoteDeletionProtectionHandlesRemoteDeletion()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };
        auto deleteRequests = 0;
        fakeFolder.setServerOverride([&deleteRequests](QNetworkAccessManager::Operation operation,
                                                        const QNetworkRequest &request,
                                                        QIODevice *) -> QNetworkReply * {
            if (operation == QNetworkAccessManager::DeleteOperation
                || request.attribute(QNetworkRequest::CustomVerbAttribute).toString() == QStringLiteral("DELETE")) {
                ++deleteRequests;
            }
            return nullptr;
        });

        fakeFolder.remoteModifier().remove(QStringLiteral("A"));
        QVERIFY(fakeFolder.syncJournal().armRemoteDeletionProtection({ QStringLiteral("A") }));

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(deleteRequests, 0);
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QVERIFY(fakeFolder.currentRemoteState().find(QStringLiteral("A")) == nullptr);
        const auto roots = fakeFolder.syncJournal().pendingRemoteDeletionProtectionRoots();
        QVERIFY(roots);
        QVERIFY(roots->isEmpty());
    }

    void remoteDeletionProtectionAllowsRecreatedLocalFileAfterRelease()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };
        auto deleteRequests = 0;
        fakeFolder.setServerOverride([&deleteRequests](QNetworkAccessManager::Operation operation,
                                                        const QNetworkRequest &request,
                                                        QIODevice *) -> QNetworkReply * {
            if (operation == QNetworkAccessManager::DeleteOperation
                || request.attribute(QNetworkRequest::CustomVerbAttribute).toString() == QStringLiteral("DELETE")) {
                ++deleteRequests;
            }
            return nullptr;
        });

        fakeFolder.remoteModifier().remove(QStringLiteral("A/a1"));
        QVERIFY(fakeFolder.syncOnce());
        const auto roots = fakeFolder.syncJournal().pendingRemoteDeletionProtectionRoots();
        QVERIFY(roots);
        QVERIFY(roots->isEmpty());

        fakeFolder.localModifier().insert(QStringLiteral("A/a1"));
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(deleteRequests, 0);
        QVERIFY(fakeFolder.currentRemoteState().find(QStringLiteral("A/a1")));
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void remoteDeletionProtectionHandlesRemovedDescendantsWithoutDirectoryMarker()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };
        auto deleteRequests = 0;
        fakeFolder.setServerOverride([&deleteRequests](QNetworkAccessManager::Operation operation,
                                                        const QNetworkRequest &request,
                                                        QIODevice *) -> QNetworkReply * {
            if (operation == QNetworkAccessManager::DeleteOperation
                || request.attribute(QNetworkRequest::CustomVerbAttribute).toString() == QStringLiteral("DELETE")) {
                ++deleteRequests;
            }
            return nullptr;
        });

        fakeFolder.remoteModifier().remove(QStringLiteral("A/a1"));
        fakeFolder.remoteModifier().remove(QStringLiteral("A/a2"));
        QVERIFY(fakeFolder.syncJournal().armRemoteDeletionProtection({ QStringLiteral("A/a1"), QStringLiteral("A/a2") }));
        fakeFolder.localModifier().remove(QStringLiteral("A/a1"));
        fakeFolder.localModifier().remove(QStringLiteral("A/a2"));

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(deleteRequests, 0);
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A")));
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("A/a1")));
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("A/a2")));
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        const auto roots = fakeFolder.syncJournal().pendingRemoteDeletionProtectionRoots();
        QVERIFY(roots);
        QVERIFY(roots->isEmpty());
    }

    void remoteDeletionProtectionDoesNotBlockOutsideDeletion()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };
        auto deleteRequests = 0;
        fakeFolder.setServerOverride([&deleteRequests](QNetworkAccessManager::Operation operation,
                                                        const QNetworkRequest &request,
                                                        QIODevice *) -> QNetworkReply * {
            if (operation == QNetworkAccessManager::DeleteOperation
                || request.attribute(QNetworkRequest::CustomVerbAttribute).toString() == QStringLiteral("DELETE")) {
                ++deleteRequests;
            }
            return nullptr;
        });

        QVERIFY(fakeFolder.syncJournal().armRemoteDeletionProtection({ QStringLiteral("A/a1") }));
        fakeFolder.localModifier().remove(QStringLiteral("A/a1"));
        fakeFolder.localModifier().remove(QStringLiteral("B/b1"));

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(deleteRequests, 1);
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A/a1")));
        QVERIFY(!fakeFolder.currentRemoteState().find(QStringLiteral("B/b1")));
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void remoteDeletionProtectionAllowsNewLocalFile()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };
        QVERIFY(fakeFolder.syncJournal().armRemoteDeletionProtection({ QStringLiteral("A") }));
        fakeFolder.localModifier().insert(QStringLiteral("A/new"));

        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(fakeFolder.currentRemoteState().find(QStringLiteral("A/new")));
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        const auto roots = fakeFolder.syncJournal().pendingRemoteDeletionProtectionRoots();
        QVERIFY(roots);
        QVERIFY(roots->isEmpty());
    }

    void remoteDeletionProtectionHandlesConflict()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };
        auto deleteRequests = 0;
        fakeFolder.setServerOverride([&deleteRequests](QNetworkAccessManager::Operation operation,
                                                        const QNetworkRequest &request,
                                                        QIODevice *) -> QNetworkReply * {
            if (operation == QNetworkAccessManager::DeleteOperation
                || request.attribute(QNetworkRequest::CustomVerbAttribute).toString() == QStringLiteral("DELETE")) {
                ++deleteRequests;
            }
            return nullptr;
        });

        QVERIFY(fakeFolder.syncJournal().armRemoteDeletionProtection({ QStringLiteral("A") }));
        fakeFolder.localModifier().appendByte(QStringLiteral("A/a1"));
        fakeFolder.remoteModifier().appendByte(QStringLiteral("A/a1"));
        fakeFolder.remoteModifier().appendByte(QStringLiteral("A/a1"));

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(deleteRequests, 0);
        QVERIFY(fakeFolder.currentRemoteState().find(QStringLiteral("A/a1")));
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("A/a1")));
        auto localState = fakeFolder.currentLocalState();
        QVERIFY(findConflict(localState, QStringLiteral("A/a1")));
        const auto roots = fakeFolder.syncJournal().pendingRemoteDeletionProtectionRoots();
        QVERIFY(roots);
        QVERIFY(roots->isEmpty());
    }

    void remoteDeletionProtectionFailsClosedWhenArmFails()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };
        const auto initialLocalState = fakeFolder.currentLocalState();
        auto deleteRequests = 0;
        fakeFolder.setServerOverride([&deleteRequests](QNetworkAccessManager::Operation operation,
                                                        const QNetworkRequest &request,
                                                        QIODevice *) -> QNetworkReply * {
            if (operation == QNetworkAccessManager::DeleteOperation
                || request.attribute(QNetworkRequest::CustomVerbAttribute).toString() == QStringLiteral("DELETE")) {
                ++deleteRequests;
            }
            return nullptr;
        });
        QObject::connect(&fakeFolder.syncEngine(), &SyncEngine::started, &fakeFolder.syncEngine(), [&fakeFolder] {
            fakeFolder.syncJournal().autotestFailCounter = 0;
        });

        fakeFolder.remoteModifier().remove(QStringLiteral("A/a1"));
        QVERIFY(!fakeFolder.syncOnce());
        QCOMPARE(deleteRequests, 0);
        QCOMPARE(fakeFolder.currentLocalState(), initialLocalState);
        fakeFolder.syncJournal().autotestFailCounter = -1;
        const auto roots = fakeFolder.syncJournal().pendingRemoteDeletionProtectionRoots();
        QVERIFY(roots);
        QVERIFY(roots->isEmpty());
    }

};

QTEST_GUILESS_MAIN(TestSyncDelete)
#include "testsyncdelete.moc"
