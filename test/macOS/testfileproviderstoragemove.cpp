/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "gui/macOS/fileproviderstoragemove.h"

#include <QList>
#include <QPair>
#include <QTest>

using namespace OCC::Mac::FileProviderStorageMove;
using namespace Qt::StringLiterals;

class TestFileProviderStorageMove : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void success()
    {
        QList<bool> enableCalls;
        QList<QPair<QString, QByteArray>> storageCalls;

        const auto result = execute(
            u"OLD"_s,
            QByteArray("old-bookmark"),
            u"NEW"_s,
            QByteArray("new-bookmark"),
            [&](const bool enabled) {
                enableCalls.append(enabled);
                return ActionResult::Changed;
            },
            [&](const QString &uuid, const QByteArray &bookmark) {
                storageCalls.append({uuid, bookmark});
            });

        const QList<QPair<QString, QByteArray>> expectedStorageCalls{
            {u"NEW"_s, QByteArray("new-bookmark")},
        };
        QCOMPARE(result.outcome, Outcome::Success);
        QCOMPARE(enableCalls, QList<bool>({false, true}));
        QCOMPARE(storageCalls, expectedStorageCalls);
    }

    void disableFailureLeavesStorageUnchanged()
    {
        QList<QPair<QString, QByteArray>> storageCalls;

        const auto result = execute(
            u"OLD"_s,
            QByteArray("old-bookmark"),
            u"NEW"_s,
            QByteArray("new-bookmark"),
            [](const bool) {
                return ActionResult::Failed;
            },
            [&](const QString &uuid, const QByteArray &bookmark) {
                storageCalls.append({uuid, bookmark});
            });

        QCOMPARE(result.outcome, Outcome::DisableFailed);
        QVERIFY(storageCalls.isEmpty());
    }

    void enableFailureRestoresPreviousStorageAndDomain()
    {
        auto enableCall = 0;
        QList<QPair<QString, QByteArray>> storageCalls;

        const auto result = execute(
            u"OLD"_s,
            QByteArray("old-bookmark"),
            u"NEW"_s,
            QByteArray("new-bookmark"),
            [&](const bool enabled) {
                ++enableCall;
                if (!enabled) {
                    return ActionResult::Changed;
                }
                return enableCall == 2 ? ActionResult::Failed : ActionResult::Changed;
            },
            [&](const QString &uuid, const QByteArray &bookmark) {
                storageCalls.append({uuid, bookmark});
            });

        const QList<QPair<QString, QByteArray>> expectedStorageCalls{
            {u"NEW"_s, QByteArray("new-bookmark")},
            {u"OLD"_s, QByteArray("old-bookmark")},
        };
        QCOMPARE(result.outcome, Outcome::EnableFailedRolledBack);
        QCOMPARE(storageCalls, expectedStorageCalls);
    }

    void rollbackFailureIsReported()
    {
        auto enableCall = 0;

        const auto result = execute(
            u"OLD"_s,
            QByteArray("old-bookmark"),
            u"NEW"_s,
            QByteArray("new-bookmark"),
            [&](const bool enabled) {
                ++enableCall;
                return enabled ? ActionResult::Failed : ActionResult::Changed;
            },
            [](const QString &, const QByteArray &) { });

        QCOMPARE(result.outcome, Outcome::RollbackFailed);
        QCOMPARE(result.enableAction, ActionResult::Failed);
        QCOMPARE(result.rollbackAction, ActionResult::Failed);
    }
};

QTEST_GUILESS_MAIN(TestFileProviderStorageMove)
#include "testfileproviderstoragemove.moc"
