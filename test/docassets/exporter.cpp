/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "exporter.h"
#include "iconexport.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QLoggingCategory>
#include <QProcess>
#include <QScopeGuard>
#include <QSet>
#include <QTemporaryDir>
#include <QUuid>

namespace OCC::DocAssets
{
namespace
{
constexpr auto diagnosticLimit = 32768;
constexpr auto killTimeoutMs = 3000;
Q_LOGGING_CATEGORY(lcExport, "nextcloud.docassets.export")
}
bool exportCatalogue(const QString &executable, const QString &output, const QStringList &scenarios, QString *error, int timeoutMs, bool includeIcons)
{
    error->clear();
    const QFileInfo destination(QDir::cleanPath(output));
    if (output.isEmpty() || !destination.isAbsolute() || destination.exists() || destination.isSymLink() || !destination.dir().exists()) {
        *error = QStringLiteral("Output must be a new absolute folder with an existing parent");
        return false;
    }
    QSet<QString> filenames;
    for (const auto &scenario : scenarios) {
        const auto name = captureFilename(scenario);
        if (name.isEmpty() || filenames.contains(name)) {
            *error = QStringLiteral("Unknown or duplicate scenario: %1").arg(scenario);
            return false;
        }
        filenames.insert(name);
    }
    if (filenames.isEmpty()) {
        *error = QStringLiteral("No scenarios requested");
        return false;
    }
    if (includeIcons) {
        for (const auto &name : iconFilenames(statusIconSources())) {
            if (filenames.contains(name)) {
                *error = QStringLiteral("Duplicate icon filename: %1").arg(name);
                return false;
            }
            filenames.insert(name);
        }
    }
    QTemporaryDir staging(destination.dir().filePath(QStringLiteral(".nextcloud-docassets-XXXXXX")));
    if (!staging.isValid()) {
        *error = QStringLiteral("Cannot create staging folder");
        return false;
    }
    const auto preservePartial = qScopeGuard([&] {
        if (error->isEmpty()) {
            return;
        }
        auto validFiles = 0;
        const QDir directory(staging.path());
        for (const auto &name : directory.entryList(QDir::Files)) {
            const auto path = directory.filePath(name);
            const auto valid = QFileInfo(path).suffix() == QStringLiteral("png") ? !QImageReader(path, "PNG").read().isNull() : QFileInfo(path).size() > 0;
            if (filenames.contains(name) && valid) {
                ++validFiles;
            } else {
                QFile::remove(path);
            }
        }
        if (validFiles == 0) {
            return;
        }
        const auto partial = destination.absoluteFilePath() + QStringLiteral("-incomplete-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
        if (QDir().rename(staging.path(), partial)) {
            staging.setAutoRemove(false);
            *error += QStringLiteral("\nRetained %1 completed files in: %2").arg(validFiles).arg(partial);
        } else {
            staging.setAutoRemove(false);
            *error += QStringLiteral("\nCould not move incomplete export; completed files remain in: %1").arg(staging.path());
        }
    });
    QStringList failures;
    for (const auto &scenario : scenarios) {
        qCInfo(lcExport).noquote() << "Capturing" << scenario;
        const auto recordFailure = [&](const QString &message) {
            failures.append(message);
            QFile::remove(staging.filePath(captureFilename(scenario)));
            qCWarning(lcExport).noquote() << message;
        };
        QProcess worker;
        worker.setProcessChannelMode(QProcess::MergedChannels);
        QByteArray diagnostics;
        const auto drain = [&] {
            diagnostics.append(worker.readAll());
            diagnostics = diagnostics.right(diagnosticLimit);
        };
        QObject::connect(&worker, &QProcess::readyRead, &worker, drain);
        worker.start(executable, {QStringLiteral("--capture-worker"), scenario, staging.path()});
        if (!worker.waitForStarted(workerStartupTimeoutMs)) {
            recordFailure(QStringLiteral("%1: worker could not start: %2").arg(scenario, worker.errorString()));
            continue;
        }
        if (!worker.waitForFinished(qMax(0, timeoutMs))) {
            worker.kill();
            worker.waitForFinished(killTimeoutMs);
            drain();
            recordFailure(QStringLiteral("%1: worker timed out\n%2").arg(scenario, QString::fromUtf8(diagnostics)));
            continue;
        }
        drain();
        if (worker.exitStatus() == QProcess::NormalExit && worker.exitCode() == captureSkippedExitCode && scenario == QStringLiteral("assistant-chat")) {
            filenames.remove(captureFilename(scenario));
            qCInfo(lcExport).noquote() << "Skipped assistant-chat: Assistant is not available for this account.";
            continue;
        }
        if (worker.exitStatus() != QProcess::NormalExit || worker.exitCode() != 0) {
            recordFailure(QStringLiteral("%1: worker failed (exit %2)\n%3").arg(scenario).arg(worker.exitCode()).arg(QString::fromUtf8(diagnostics)));
            continue;
        }
        if (QImageReader(staging.filePath(captureFilename(scenario)), "PNG").read().isNull()) {
            recordFailure(QStringLiteral("%1: missing or invalid PNG").arg(scenario));
            continue;
        }
    }
    if (includeIcons) {
        QString iconError;
        if (!exportIcons(staging.path(), statusIconSources(), &iconError)) {
            failures.append(iconError);
        }
    }
    if (!failures.isEmpty()) {
        *error = failures.join(u'\n');
        return false;
    }
    const auto entries = QDir(staging.path()).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot);
    if (QSet<QString>(entries.begin(), entries.end()) != filenames) {
        *error = QStringLiteral("Unexpected files in staged export");
        return false;
    }
    if (!QDir().rename(staging.path(), destination.absoluteFilePath())) {
        *error = QStringLiteral("Could not publish completed export");
        return false;
    }
    staging.setAutoRemove(false);
    return true;
}
}
