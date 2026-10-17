/*
 * SPDX-FileCopyrightText: 2026 Nextcloud contributors
 * SPDX-License-Identifier: CC0-1.0
 */

#include "configfile.h"
#include "syncenginetestutils.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTextStream>
#include <cstdio>

using namespace OCC;
using namespace Qt::StringLiterals;

// Compare the same invocation on the baseline and patched builds. Timings stop
// at aboutToPropagate; propagation still completes to check the resulting sync.
int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    const auto args = app.arguments();
    const auto filesPerDirectory = args.size() > 1 ? args[1].toInt() : 5000;
    const auto directories = args.size() > 2 ? args[2].toInt() : 1;
    const auto virtualFiles = args.size() <= 3 || args[3] != "regular"_L1;
    const auto parallelJobs = args.size() > 4 ? args[4].toInt() : 5;
    if (filesPerDirectory < 1 || filesPerDirectory > 20000 || directories < 1 || directories > 20000 || (filesPerDirectory + 1) * directories > 100000
        || parallelJobs < 1 || parallelJobs > 5) {
        return 2;
    }

    QTemporaryDir benchmarkDirectory;
    if (!benchmarkDirectory.isValid() || !ConfigFile::setConfDir(benchmarkDirectory.path())) {
        return 2;
    }
    FakeFolder folder{FileInfo::A12_B12_C12_S12()};
    auto options = folder.syncEngine().syncOptions();
    options._parallelNetworkJobs = parallelJobs;
    folder.syncEngine().setSyncOptions(options);
    Logger::instance()->setLogFile(benchmarkDirectory.filePath(u"discovery.log"_s));
    Logger::instance()->setLogFlush(false);
    if (virtualFiles) {
        auto vfs = QSharedPointer<Vfs>(createVfsFromPlugin(Vfs::WithSuffix).release());
        if (!vfs) {
            Logger::instance()->setLogFile(QString());
            return 2;
        }
        folder.switchToVfs(vfs);
        folder.syncJournal().internalPinStates().setForPath("", PinState::Unspecified);
    }
    for (auto directory = 0; directory < directories; ++directory) {
        const auto path = u"batch-%1"_s.arg(directory, 3, 10, QLatin1Char('0'));
        folder.remoteModifier().mkdir(path);
        for (auto file = 0; file < filesPerDirectory; ++file) {
            folder.remoteModifier().insert(path + u"/file-%1"_s.arg(file, 6, 10, QLatin1Char('0')));
        }
    }

    QElapsedTimer elapsed;
    qint64 previousHeartbeat = 0;
    qint64 maximumHeartbeatGap = 0;
    qint64 discoveryMilliseconds = -1;
    qint64 healthRequestStarted = -1;
    qint64 healthReplyMilliseconds = -1;
    auto healthReplyBeforePropagation = false;
    auto healthReplyValid = false;
    auto directoryRequests = 0;
    auto heartbeatCount = 0;
    auto discoveredFiles = 0;
    folder.setServerOverride([&](QNetworkAccessManager::Operation, const QNetworkRequest &request, QIODevice *) -> QNetworkReply * {
        if (request.attribute(QNetworkRequest::CustomVerbAttribute).toString() == "PROPFIND"_L1 && request.rawHeader("Depth") == "1") {
            ++directoryRequests;
        }
        return nullptr;
    });
    QTimer heartbeat;
    heartbeat.setTimerType(Qt::PreciseTimer);
    heartbeat.setInterval(1);
    const auto sampleHeartbeat = [&] {
        const auto now = elapsed.elapsed();
        maximumHeartbeatGap = qMax(maximumHeartbeatGap, now - previousHeartbeat);
        previousHeartbeat = now;
        ++heartbeatCount;
    };
    QObject::connect(&heartbeat, &QTimer::timeout, &app, sampleHeartbeat);
    QObject::connect(&folder.syncEngine(), &SyncEngine::itemDiscovered, &app, [&](const SyncFileItemPtr &item) {
        if (!item->isDirectory() && item->_file.startsWith("batch-"_L1)) {
            ++discoveredFiles;
            if (discoveredFiles == 1) {
                healthRequestStarted = elapsed.elapsed();
                auto *health = new PropfindJob(folder.account(), u"/"_s, &folder.syncEngine());
                health->setProperties({"getlastmodified"_ba});
                QObject::connect(health, &PropfindJob::result, &folder.syncEngine(), [&](const QVariantMap &properties) {
                    healthReplyMilliseconds = elapsed.elapsed() - healthRequestStarted;
                    healthReplyBeforePropagation = discoveryMilliseconds < 0;
                    healthReplyValid = properties.contains(u"getlastmodified"_s);
                });
                health->start();
            }
        }
    });
    QObject::connect(&folder.syncEngine(), &SyncEngine::aboutToPropagate, &app, [&](SyncFileItemVector &) {
        sampleHeartbeat();
        discoveryMilliseconds = elapsed.elapsed();
        heartbeat.stop();
    });
    elapsed.start();
    heartbeat.start();
    const auto success = folder.syncOnce();
    heartbeat.stop();
    const auto syncMilliseconds = elapsed.elapsed();
    Logger::instance()->setLogFile(QString());
    const QJsonObject result{
        {u"success"_s, success},
        {u"virtual_files"_s, virtualFiles},
        {u"parallel_jobs"_s, parallelJobs},
        {u"directory_requests"_s, directoryRequests},
        {u"directories"_s, directories},
        {u"files_per_directory"_s, filesPerDirectory},
        {u"discovered_files"_s, discoveredFiles},
        {u"discovery_ms"_s, discoveryMilliseconds},
        {u"maximum_heartbeat_gap_ms"_s, maximumHeartbeatGap},
        {u"heartbeat_samples"_s, heartbeatCount},
        {u"root_health_reply_ms"_s, healthReplyMilliseconds},
        {u"root_health_reply_valid"_s, healthReplyValid},
        {u"root_health_reply_before_propagation"_s, healthReplyBeforePropagation},
        {u"transport"_s, u"queued fake replies; no socket or server latency"_s},
        {u"complete_sync_ms"_s, syncMilliseconds},
    };
    QTextStream(stdout) << QJsonDocument(result).toJson(QJsonDocument::Compact) << Qt::endl;
    return success && healthReplyValid && discoveredFiles == filesPerDirectory * directories ? 0 : 1;
}
