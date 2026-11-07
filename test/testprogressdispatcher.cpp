/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "progressdispatcher.h"

#include <QtTest>

#include <limits>

using namespace OCC;
using namespace Qt::StringLiterals;

class TestProgressDispatcher : public QObject
{
    Q_OBJECT

    static SyncFileItem makeItem(qint64 size)
    {
        auto item = SyncFileItem{};
        item._file = u"file"_s;
        item._type = ItemTypeFile;
        item._instruction = CSYNC_INSTRUCTION_NEW;
        item._size = size;
        return item;
    }

    static void updateEstimates(ProgressInfo &progress, int count = 1)
    {
        for (auto tick = 0; tick < count; ++tick) {
            QVERIFY(QMetaObject::invokeMethod(&progress, "updateEstimates", Qt::DirectConnection));
        }
    }

    static constexpr auto maximumEta = quint64(std::numeric_limits<qint64>::max());

private Q_SLOTS:
    void estimates_data()
    {
        QTest::addColumn<qint64>("total");
        QTest::addColumn<qint64>("completed");
        QTest::addColumn<quint64>("expectedEta");

        const auto maximumSeconds = std::numeric_limits<qint64>::max() / 1000;
        QTest::newRow("zero-rate") << qint64(100) << qint64(0) << quint64(0);
        QTest::newRow("round-down") << qint64(24) << qint64(10) << quint64(1000);
        QTest::newRow("round-up") << qint64(25) << qint64(10) << quint64(2000);
        QTest::newRow("completed") << qint64(100) << qint64(100) << quint64(0);
        QTest::newRow("large-representable") << qint64(9000000000000001) << qint64(1) << quint64(9000000000000000000);
        QTest::newRow("below-limit") << maximumSeconds << qint64(1) << quint64((maximumSeconds - 1) * 1000);
        QTest::newRow("rounded-to-limit") << maximumSeconds + 1 << qint64(1) << maximumEta;
        QTest::newRow("millisecond-overflow") << qint64(10000000000000001) << qint64(1) << maximumEta;
        QTest::newRow("second-overflow") << std::numeric_limits<qint64>::max() << qint64(1) << maximumEta;
    }

    void estimates()
    {
        QFETCH(qint64, total);
        QFETCH(qint64, completed);
        QFETCH(quint64, expectedEta);

        auto progress = ProgressInfo{};
        const auto item = makeItem(total);
        progress.adjustTotalsForFile(item);
        progress.setProgressItem(item, completed);
        updateEstimates(progress);

        const auto estimate = progress.fileProgress(item);
        QCOMPARE(estimate.estimatedBandwidth, completed);
        QCOMPARE(estimate.estimatedEta, expectedEta);
    }

    void stalledTransfer_data()
    {
        QTest::addColumn<int>("idleUpdates");

        QTest::newRow("millisecond-overflow") << 254;
        QTest::newRow("second-overflow") << 319;
        QTest::newRow("infinite-seconds") << 8000;
    }

    void stalledTransfer()
    {
        QFETCH(int, idleUpdates);

        auto progress = ProgressInfo{};
        const auto item = makeItem(1001000000);
        progress.adjustTotalsForFile(item);
        progress.setProgressItem(item, 1000000);
        updateEstimates(progress);
        updateEstimates(progress, idleUpdates);

        QCOMPARE(progress.fileProgress(item).estimatedEta, maximumEta);
        QVERIFY(progress.totalProgress().estimatedEta >= maximumEta);
        QVERIFY(!progress.trustEta());

        progress.setProgressItem(item, 2000000);
        updateEstimates(progress);

        const auto resumed = progress.totalProgress();
        QVERIFY(resumed.estimatedBandwidth > 0);
        QVERIFY(resumed.estimatedEta > 0);
        QVERIFY(resumed.estimatedEta < maximumEta);
        QVERIFY(progress.trustEta());

        progress.setProgressComplete(item);
        updateEstimates(progress);

        QCOMPARE(progress.totalProgress().estimatedEta, quint64(0));
        QCOMPARE(progress.completedSize(), progress.totalSize());
        QCOMPARE(progress.completedFiles(), progress.totalFiles());
    }

    void stalledFileCount()
    {
        auto progress = ProgressInfo{};
        auto completed = makeItem(0);
        completed._instruction = CSYNC_INSTRUCTION_RENAME;
        auto pending = completed;
        pending._file = u"pending"_s;
        pending._affectedItems = 10;
        progress.adjustTotalsForFile(completed);
        progress.adjustTotalsForFile(pending);
        progress.setProgressComplete(completed);
        updateEstimates(progress);
        updateEstimates(progress, 400);

        QCOMPARE(progress.totalSize(), qint64(0));
        QCOMPARE(progress.totalProgress().estimatedEta, maximumEta);
        QVERIFY(!progress.trustEta());

        progress.setProgressComplete(pending);
        updateEstimates(progress);

        QCOMPARE(progress.totalProgress().estimatedEta, quint64(0));
        QCOMPARE(progress.completedFiles(), progress.totalFiles());
    }
};

QTEST_GUILESS_MAIN(TestProgressDispatcher)
#include "testprogressdispatcher.moc"
