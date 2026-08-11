/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "uiscreenshotmanifest.h"
#include "uiscreenshotmanifestdata.h"

namespace OCC::UiScreenshots {

namespace {

struct ParsedManifest
{
    QList<QmlScreenshotJob> qmlJobs;
    QList<NativeScreenshotJob> nativeJobs;
    QList<std::pair<QString, QString>> svgMappings;
    QStringList excludedPngFiles;
    QStringList whiteSvgFiles;
};

QmlScreenshotJobKind qmlJobKind(const QString &name)
{
    if (name == QStringLiteral("Activities")) {
        return QmlScreenshotJobKind::Activities;
    }
    if (name == QStringLiteral("UserStatus")) {
        return QmlScreenshotJobKind::UserStatus;
    }
    if (name == QStringLiteral("Assistant")) {
        return QmlScreenshotJobKind::Assistant;
    }
    if (name == QStringLiteral("WizardServer")) {
        return QmlScreenshotJobKind::WizardServer;
    }
    if (name == QStringLiteral("WizardBrowserAuth")) {
        return QmlScreenshotJobKind::WizardBrowserAuth;
    }
    if (name == QStringLiteral("WizardSyncOptions")) {
        return QmlScreenshotJobKind::WizardSyncOptions;
    }
    qFatal("Unknown QML screenshot job kind in manifest: %s", qPrintable(name));
    return QmlScreenshotJobKind::Activities;
}

NativeScreenshotJobKind nativeJobKind(const QString &name)
{
    if (name == QStringLiteral("User")) {
        return NativeScreenshotJobKind::User;
    }
    if (name == QStringLiteral("General")) {
        return NativeScreenshotJobKind::General;
    }
    if (name == QStringLiteral("Advanced")) {
        return NativeScreenshotJobKind::Advanced;
    }
    if (name == QStringLiteral("Info")) {
        return NativeScreenshotJobKind::Info;
    }
    if (name == QStringLiteral("Network")) {
        return NativeScreenshotJobKind::Network;
    }
    if (name == QStringLiteral("IgnoredFiles")) {
        return NativeScreenshotJobKind::IgnoredFiles;
    }
    qFatal("Unknown native screenshot job kind in manifest: %s", qPrintable(name));
    return NativeScreenshotJobKind::User;
}

const ParsedManifest &parsedManifest()
{
    static const auto manifest = [] {
        auto result = ParsedManifest{};
        const auto lines = QString::fromUtf8(ManifestData::data).split(QLatin1Char('\n'));
        for (const auto &rawLine : lines) {
            const auto line = rawLine.trimmed();
            if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
                continue;
            }
            const auto fields = line.split(QLatin1Char('\t'));
            if (fields.size() != 4) {
                qFatal("Malformed UI screenshot manifest line: %s", qPrintable(line));
            }

            const auto &category = fields.at(0);
            const auto &kind = fields.at(1);
            const auto &output = fields.at(2);
            const auto &source = fields.at(3);
            if (category == QStringLiteral("qml")) {
                result.qmlJobs.append({qmlJobKind(kind), output, QUrl(source)});
            } else if (category == QStringLiteral("native")) {
                result.nativeJobs.append({nativeJobKind(kind), output});
            } else if (category == QStringLiteral("svg")) {
                result.svgMappings.append({source, output});
            } else if (category == QStringLiteral("excluded-png")) {
                result.excludedPngFiles.append(output);
            } else if (category == QStringLiteral("white-svg")) {
                result.whiteSvgFiles.append(output);
            } else {
                qFatal("Unknown UI screenshot manifest category: %s", qPrintable(category));
            }
        }
        return result;
    }();
    return manifest;
}

}

const QList<QmlScreenshotJob> &qmlScreenshotJobs()
{
    return parsedManifest().qmlJobs;
}

const QList<NativeScreenshotJob> &nativeScreenshotJobs()
{
    return parsedManifest().nativeJobs;
}

QString nativeScreenshotOutputName(const NativeScreenshotJobKind kind)
{
    for (const auto &job : nativeScreenshotJobs()) {
        if (job.kind == kind) {
            return job.outputName;
        }
    }
    return {};
}

const QList<std::pair<QString, QString>> &svgResourceMappings()
{
    return parsedManifest().svgMappings;
}

const QStringList &excludedPngFileNames()
{
    return parsedManifest().excludedPngFiles;
}

const QStringList &whiteSvgFileNames()
{
    return parsedManifest().whiteSvgFiles;
}

}
