/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef UISCREENSHOTMANIFEST_H
#define UISCREENSHOTMANIFEST_H

#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <utility>

namespace OCC::UiScreenshots {

/** @brief Identifies one production QML surface captured by the screenshot workflow. */
enum class QmlScreenshotJobKind {
    Activities,
    UserStatus,
    Assistant,
    WizardServer,
    WizardBrowserAuth,
    WizardSyncOptions,
};

/** @brief Describes one QML screenshot job and its allowlisted output. */
struct QmlScreenshotJob
{
    QmlScreenshotJobKind kind; //!< Fixture state selected before component creation.
    QString outputName; //!< Allowlisted PNG output name.
    QUrl componentUrl; //!< Production QML window URL.
};

/** @brief Identifies one production QWidget surface captured by the screenshot workflow. */
enum class NativeScreenshotJobKind {
    User,
    General,
    Advanced,
    Info,
    Network,
    IgnoredFiles,
    Finished,
};

/** @brief Describes one native screenshot job and its allowlisted output. */
struct NativeScreenshotJob
{
    NativeScreenshotJobKind kind; //!< Production Settings surface to select.
    QString outputName; //!< Allowlisted PNG output name.
};

/** @brief Returns the ordered immutable QML screenshot jobs. */
[[nodiscard]] const QList<QmlScreenshotJob> &qmlScreenshotJobs();

/** @brief Returns the ordered immutable native screenshot jobs. */
[[nodiscard]] const QList<NativeScreenshotJob> &nativeScreenshotJobs();

/** @brief Returns the output name for @p kind, or an empty string for phase completion. */
[[nodiscard]] QString nativeScreenshotOutputName(NativeScreenshotJobKind kind);

/** @brief Returns the immutable production-resource to output-file SVG mapping. */
[[nodiscard]] const QList<std::pair<QString, QString>> &svgResourceMappings();

/** @brief Returns explicitly excluded legacy PNG names. */
[[nodiscard]] const QStringList &excludedPngFileNames();

/** @brief Returns the forbidden white-state SVG names. */
[[nodiscard]] const QStringList &whiteSvgFileNames();

}

#endif // UISCREENSHOTMANIFEST_H
