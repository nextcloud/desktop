/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef UISCREENSHOTOUTPUT_H
#define UISCREENSHOTOUTPUT_H

#include <QByteArray>
#include <QImage>
#include <QString>
#include <QStringList>

#include <functional>

namespace OCC {

/** @brief Owns safe staging, fixed manifests, atomic writes, and run markers for screenshots. */
class UiScreenshotOutput
{
public:
    using AtomicWriter = std::function<bool(const QString &, const QByteArray &, QString *)>;

    /**
     * @brief Creates an output boundary for one screenshot phase.
     * @param directory Fixed staging directory supplied by the Xcode scheme.
     * @param expectedTmpRoot Expected app-container `Data/tmp` root. Empty derives it from the process home.
     * @param atomicWriter Optional writer seam used by focused automated tests.
     */
    explicit UiScreenshotOutput(QString directory, QString expectedTmpRoot = {}, AtomicWriter atomicWriter = {});

    /** @brief Returns the validated staging directory supplied at construction. */
    [[nodiscard]] const QString &directory() const;

    /** @brief Returns the expected app-container `Data/tmp` root for @p homePath. */
    [[nodiscard]] static QString expectedTmpRootForHome(const QString &homePath);

    /**
     * @brief Validates a fixed staging path without changing the filesystem.
     * @param directory Candidate staging directory.
     * @param expectedTmpRoot Expected app-container `Data/tmp` parent.
     * @param error Receives a precise failure description.
     */
    [[nodiscard]] static bool validateStagingPath(const QString &directory, const QString &expectedTmpRoot, QString *error);

    /** @brief Validates the configured staging path without changing the filesystem. */
    [[nodiscard]] bool validate(QString *error) const;

    /**
     * @brief Starts a new QML run after fixed allowlist cleanup.
     * @param runId Receives the newly generated UUID.
     * @param error Receives a precise failure description.
     */
    [[nodiscard]] bool beginQmlRun(QString *runId, QString *error);

    /**
     * @brief Validates QML completion and prepares only the native-owned files.
     * @param runId UUID supplied by the orchestration script.
     * @param error Receives a precise failure description.
     */
    [[nodiscard]] bool beginNativeRun(const QString &runId, QString *error);

    /** @brief Atomically writes an allowlisted PNG and verifies the result. */
    [[nodiscard]] bool writePng(const QString &fileName, const QImage &image, QString *error) const;

    /** @brief Copies all fixed production SVG resources atomically into their output names. */
    [[nodiscard]] bool exportStatusSvgs(QString *error) const;

    /** @brief Verifies all QML-owned outputs and atomically writes `.qml-complete`. */
    [[nodiscard]] bool completeQmlRun(const QString &runId, QString *error) const;

    /** @brief Verifies all native-owned outputs and atomically writes `.native-complete`. */
    [[nodiscard]] bool completeNativeRun(const QString &runId, QString *error) const;

    /** @brief Returns whether @p runId is a canonical UUID without braces. */
    [[nodiscard]] static bool isValidRunId(const QString &runId);

    /** @brief Returns the six QML-owned PNG names. */
    [[nodiscard]] static QStringList qmlPngFiles();

    /** @brief Returns the six native-owned PNG names. */
    [[nodiscard]] static QStringList nativePngFiles();

    /** @brief Returns the twelve exported SVG names. */
    [[nodiscard]] static QStringList svgFiles();

    /** @brief Returns all twenty-four deliverable names. */
    [[nodiscard]] static QStringList deliverableFiles();

    /** @brief Returns explicitly excluded legacy PNG names. */
    [[nodiscard]] static QStringList excludedPngFiles();

    /** @brief Returns the six forbidden white-state SVG names. */
    [[nodiscard]] static QStringList whiteSvgFiles();

private:
    [[nodiscard]] bool ensureDirectory(QString *error) const;
    [[nodiscard]] bool rejectUnexpectedWhiteSvgs(QString *error) const;
    [[nodiscard]] bool cleanupFiles(const QStringList &fileNames, QString *error) const;
    [[nodiscard]] bool writeData(const QString &fileName, const QByteArray &data, QString *error) const;
    [[nodiscard]] bool verifyFiles(const QStringList &fileNames, QString *error) const;
    [[nodiscard]] bool verifyMarker(const QString &fileName, const QString &runId, QString *error) const;
    [[nodiscard]] bool completeRun(const QStringList &ownedFiles, const QString &markerName, const QString &runId, QString *error) const;
    [[nodiscard]] static bool defaultAtomicWrite(const QString &path, const QByteArray &data, QString *error);
    [[nodiscard]] static bool isAllowedWritableName(const QString &fileName);

    const QString _directory;
    const QString _expectedTmpRoot;
    const AtomicWriter _atomicWriter;
};

}

#endif // UISCREENSHOTOUTPUT_H
