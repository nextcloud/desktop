/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: CC0-1.0
 *
 * This software is in the public domain, furnished "as is", without technical
 * support, and with no warranty, express or implied, as to its usefulness for
 * any purpose.
 */

#include <QCoreApplication>
#include <QObject>
#include <QTimer>
#include <QEventLoop>
#include <QElapsedTimer>
#include <QtGlobal>

#include <cstdio>

// Reproduces the scheduling cost that OwncloudPropagator::scheduleNextJob()
// used to pay. The propagator serializes scheduling passes with a single
// "_jobScheduled" flag, so exactly one deferred pass is in flight at a time
// and each of the N passes for an N-file sync pays the deferral latency
// back-to-back.
//
// Old path: QTimer::singleShot(3, ...) -> 3 ms per pass.
// New path: QMetaObject::invokeMethod(..., Qt::QueuedConnection) -> next
//           event-loop iteration, no artificial delay.
//
// With rounds = 10000 the timer path takes ~30 s (10000 * 3 ms); the queued
// path completes in a few milliseconds.

namespace {
constexpr int kRounds = 10000;
constexpr int kDelayMs = 3; // value previously used in scheduleNextJob()
}

class ScheduleWorker : public QObject
{
    Q_OBJECT
public:
    enum class Mode { TimerSingleShot, QueuedInvoke };

    explicit ScheduleWorker(Mode mode, QObject *parent = nullptr)
        : QObject(parent)
        , _mode(mode)
    {
    }

    // Runs kRounds deferred scheduling passes through the configured
    // mechanism and returns the elapsed wall-clock time in milliseconds.
    qint64 run()
    {
        _count = 0;
        QEventLoop loop;
        _loop = &loop;
        QElapsedTimer timer;
        timer.start();
        scheduleNext();
        loop.exec();
        _loop = nullptr;
        return timer.elapsed();
    }

private:
    void scheduleNext()
    {
        // Mirror the propagator's "one pass in flight" model: every pass
        // schedules the next one through the deferral mechanism under test.
        if (_mode == Mode::TimerSingleShot) {
            QTimer::singleShot(kDelayMs, this, &ScheduleWorker::tick);
        } else {
            QMetaObject::invokeMethod(this, &ScheduleWorker::tick, Qt::QueuedConnection);
        }
    }

private slots:
    void tick()
    {
        if (++_count >= kRounds) {
            if (_loop) {
                _loop->quit();
            }
            return;
        }
        scheduleNext();
    }

private:
    Mode _mode;
    int _count = 0;
    QEventLoop *_loop = nullptr;
};

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    ScheduleWorker queued(ScheduleWorker::Mode::QueuedInvoke);
    const auto queuedMs = queued.run();

    ScheduleWorker timed(ScheduleWorker::Mode::TimerSingleShot);
    const auto timerMs = timed.run();

    std::printf("ROUNDS %d\n", kRounds);
    std::printf("QTimer::singleShot(%dms): %lld ms\n", kDelayMs, static_cast<long long>(timerMs));
    std::printf("QMetaObject::invokeMethod(QueuedConnection): %lld ms\n", static_cast<long long>(queuedMs));
    std::printf("SAVED: %lld ms\n", static_cast<long long>(timerMs - queuedMs));
    std::fflush(stdout);

    // Sanity: both paths scheduled the same number of rounds; the queued path
    // must be dramatically faster because it has no per-pass timer latency.
    const bool ok = timerMs > queuedMs * 10;
    return ok ? 0 : 1;
}

#include "benchscheduledelay.moc"
