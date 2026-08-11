/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "uiscreenshotoutput.h"

#include "uiscreenshotmanifest.h"
#include "uiscreenshotmode.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSet>
#include <QUuid>

#include <utility>

namespace OCC {

namespace {
constexpr auto stagingDirectoryName = "nextcloud-ui-screenshots";
constexpr auto containerHomeSuffix = "/Library/Containers/com.nextcloud.desktopclient/Data";
constexpr auto runIdMarker = ".run-id";
constexpr auto qmlCompleteMarker = ".qml-complete";
constexpr auto nativeCompleteMarker = ".native-complete";

QString cleanAbsolutePath(const QString &path)
{
    return QDir::cleanPath(QDir::fromNativeSeparators(path));
}

const QStringList &markerFiles()
{
    static const auto fileNames = QStringList{
        QString::fromLatin1(runIdMarker),
        QString::fromLatin1(qmlCompleteMarker),
        QString::fromLatin1(nativeCompleteMarker),
    };
    return fileNames;
}

const QSet<QString> &pngFileNames()
{
    static const auto fileNames = [] {
        auto names = QSet<QString>{};
        for (const auto &job : UiScreenshots::qmlScreenshotJobs()) {
            names.insert(job.outputName);
        }
        for (const auto &job : UiScreenshots::nativeScreenshotJobs()) {
            names.insert(job.outputName);
        }
        return names;
    }();
    return fileNames;
}

const QSet<QString> &writableFileNames()
{
    static const auto fileNames = [] {
        auto names = pngFileNames();
        for (const auto &mapping : UiScreenshots::svgResourceMappings()) {
            names.insert(mapping.second);
        }
        for (const auto &fileName : UiScreenshots::excludedPngFileNames()) {
            names.insert(fileName);
        }
        for (const auto &fileName : UiScreenshots::whiteSvgFileNames()) {
            names.insert(fileName);
        }
        for (const auto &fileName : markerFiles()) {
            names.insert(fileName);
        }
        return names;
    }();
    return fileNames;
}
}

UiScreenshotOutput::UiScreenshotOutput(QString directory, QString expectedTmpRoot, AtomicWriter atomicWriter)
    : _directory(directory.isEmpty() ? QString{} : cleanAbsolutePath(directory))
    , _expectedTmpRoot(expectedTmpRoot.isEmpty()
              ? expectedTmpRootForHome(QDir::homePath())
              : cleanAbsolutePath(expectedTmpRoot))
    , _atomicWriter(atomicWriter ? std::move(atomicWriter) : defaultAtomicWrite)
{
}

const QString &UiScreenshotOutput::directory() const
{
    return _directory;
}

QString UiScreenshotOutput::expectedTmpRootForHome(const QString &homePath)
{
    const auto cleanHome = cleanAbsolutePath(homePath);
    if (cleanHome.endsWith(QLatin1String(containerHomeSuffix))) {
        return QDir(cleanHome).filePath(QStringLiteral("tmp"));
    }
    return QDir(cleanHome).filePath(QStringLiteral("Library/Containers/com.nextcloud.desktopclient/Data/tmp"));
}

bool UiScreenshotOutput::validateStagingPath(const QString &directory, const QString &expectedTmpRoot, QString *error)
{
    const auto fail = [error](const QString &message) {
        if (error) {
            *error = message;
        }
        return false;
    };

    if (directory.isEmpty()) {
        return fail(QStringLiteral("NEXTCLOUD_UI_SCREENSHOT_OUTPUT is empty."));
    }
    if (directory.contains(QStringLiteral("$(")) || directory.contains(QStringLiteral("${"))
        || directory.contains(QLatin1Char('~'))) {
        return fail(QStringLiteral("Screenshot output contains an unresolved macro or '~': %1").arg(directory));
    }

    const auto candidate = cleanAbsolutePath(directory);
    const auto expectedRoot = cleanAbsolutePath(expectedTmpRoot);
    if (!QDir::isAbsolutePath(candidate)) {
        return fail(QStringLiteral("Screenshot output is not an absolute path: %1").arg(directory));
    }
    if (!QDir::isAbsolutePath(expectedRoot) || expectedRoot == QLatin1String("/")) {
        return fail(QStringLiteral("Expected app-container temporary root is invalid: %1").arg(expectedTmpRoot));
    }

    const auto expectedDirectory = QDir(expectedRoot).filePath(QString::fromLatin1(stagingDirectoryName));
    if (candidate != expectedDirectory) {
        return fail(QStringLiteral("Screenshot output must be exactly inside the app-container Data/tmp root: %1").arg(expectedDirectory));
    }

    const auto info = QFileInfo(candidate);
    if (info.isSymLink()) {
        return fail(QStringLiteral("Screenshot output directory is a symbolic link: %1").arg(candidate));
    }
    if (info.exists() && !info.isDir()) {
        return fail(QStringLiteral("Screenshot output exists but is not a directory: %1").arg(candidate));
    }

    return true;
}

bool UiScreenshotOutput::validate(QString *error) const
{
    return validateStagingPath(_directory, _expectedTmpRoot, error);
}

bool UiScreenshotOutput::beginQmlRun(QString *runId, QString *error)
{
    if (!ensureDirectory(error) || !rejectUnexpectedWhiteSvgs(error)) {
        return false;
    }

    auto staleFiles = deliverableFiles();
    staleFiles.append(excludedPngFiles());
    staleFiles.append(whiteSvgFiles());
    staleFiles.append(markerFiles());
    staleFiles.removeDuplicates();
    if (!cleanupFiles(staleFiles, error)) {
        return false;
    }

    const auto newRunId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (!writeData(QString::fromLatin1(runIdMarker), newRunId.toUtf8(), error)) {
        return false;
    }
    if (runId) {
        *runId = newRunId;
    }
    qCInfo(UiScreenshots::lcUiScreenshots) << "Started QML screenshot run" << newRunId;
    return true;
}

bool UiScreenshotOutput::beginNativeRun(const QString &runId, QString *error)
{
    if (!isValidRunId(runId)) {
        if (error) {
            *error = QStringLiteral("NEXTCLOUD_UI_SCREENSHOT_RUN_ID is not a canonical UUID.");
        }
        return false;
    }
    if (!ensureDirectory(error)
        || !verifyMarker(QString::fromLatin1(runIdMarker), runId, error)
        || !verifyMarker(QString::fromLatin1(qmlCompleteMarker), runId, error)) {
        return false;
    }

    auto staleFiles = nativePngFiles();
    staleFiles.append(QString::fromLatin1(nativeCompleteMarker));
    if (!cleanupFiles(staleFiles, error)) {
        return false;
    }
    qCInfo(UiScreenshots::lcUiScreenshots) << "Prepared native screenshot run" << runId;
    return true;
}

bool UiScreenshotOutput::writePng(const QString &fileName, const QImage &image, QString *error) const
{
    if (!pngFileNames().contains(fileName)) {
        if (error) {
            *error = QStringLiteral("PNG filename is not allowlisted: %1").arg(fileName);
        }
        return false;
    }
    if (image.isNull() || image.width() <= 0 || image.height() <= 0) {
        if (error) {
            *error = QStringLiteral("Captured image is empty for %1.").arg(fileName);
        }
        return false;
    }

    auto data = QByteArray{};
    auto buffer = QBuffer(&data);
    if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "PNG") || data.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Could not encode PNG data for %1.").arg(fileName);
        }
        return false;
    }
    return writeData(fileName, data, error);
}

bool UiScreenshotOutput::exportStatusSvgs(QString *error) const
{
    for (const auto &[resourceName, outputName] : UiScreenshots::svgResourceMappings()) {
        qCInfo(UiScreenshots::lcUiScreenshots) << "Reading SVG resource" << resourceName;
        auto source = QFile(resourceName);
        if (!source.open(QIODevice::ReadOnly)) {
            if (error) {
                *error = QStringLiteral("Could not open SVG resource %1.").arg(resourceName);
            }
            return false;
        }
        const auto data = source.readAll();
        if (data.isEmpty()) {
            if (error) {
                *error = QStringLiteral("SVG resource is empty: %1.").arg(resourceName);
            }
            return false;
        }
        if (!writeData(outputName, data, error)) {
            return false;
        }
    }
    return true;
}

bool UiScreenshotOutput::completeQmlRun(const QString &runId, QString *error) const
{
    auto ownedFiles = qmlPngFiles();
    ownedFiles.append(svgFiles());
    return completeRun(ownedFiles, QString::fromLatin1(qmlCompleteMarker), runId, error);
}

bool UiScreenshotOutput::completeNativeRun(const QString &runId, QString *error) const
{
    return completeRun(nativePngFiles(), QString::fromLatin1(nativeCompleteMarker), runId, error);
}

bool UiScreenshotOutput::isValidRunId(const QString &runId)
{
    if (runId.isEmpty() || runId.contains(QLatin1Char('{')) || runId.contains(QLatin1Char('}'))) {
        return false;
    }
    const auto uuid = QUuid(runId);
    return !uuid.isNull()
        && uuid.toString(QUuid::WithoutBraces).compare(runId, Qt::CaseInsensitive) == 0;
}

QStringList UiScreenshotOutput::qmlPngFiles()
{
    auto fileNames = QStringList{};
    fileNames.reserve(UiScreenshots::qmlScreenshotJobs().size());
    for (const auto &job : UiScreenshots::qmlScreenshotJobs()) {
        fileNames.append(job.outputName);
    }
    return fileNames;
}

QStringList UiScreenshotOutput::nativePngFiles()
{
    auto fileNames = QStringList{};
    fileNames.reserve(UiScreenshots::nativeScreenshotJobs().size());
    for (const auto &job : UiScreenshots::nativeScreenshotJobs()) {
        fileNames.append(job.outputName);
    }
    return fileNames;
}

QStringList UiScreenshotOutput::svgFiles()
{
    auto files = QStringList{};
    files.reserve(UiScreenshots::svgResourceMappings().size());
    for (const auto &mapping : UiScreenshots::svgResourceMappings()) {
        files.append(mapping.second);
    }
    return files;
}

QStringList UiScreenshotOutput::deliverableFiles()
{
    auto files = qmlPngFiles();
    files.append(nativePngFiles());
    files.append(svgFiles());
    return files;
}

QStringList UiScreenshotOutput::excludedPngFiles()
{
    return UiScreenshots::excludedPngFileNames();
}

QStringList UiScreenshotOutput::whiteSvgFiles()
{
    return UiScreenshots::whiteSvgFileNames();
}

bool UiScreenshotOutput::ensureDirectory(QString *error) const
{
    if (!validate(error)) {
        return false;
    }
    if (!QDir().mkpath(_directory)) {
        if (error) {
            *error = QStringLiteral("Could not create screenshot staging directory: %1").arg(_directory);
        }
        return false;
    }
    return validate(error);
}

bool UiScreenshotOutput::rejectUnexpectedWhiteSvgs(QString *error) const
{
    const auto entries = QDir(_directory).entryInfoList(
        {QStringLiteral("*-white.svg"), QStringLiteral(".*-white.svg")},
        QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
    for (const auto &entry : entries) {
        if (whiteSvgFiles().contains(entry.fileName())) {
            continue;
        }
        if (error) {
            *error = QStringLiteral("Unexpected white SVG exists in screenshot staging: %1").arg(entry.fileName());
        }
        return false;
    }
    return true;
}

bool UiScreenshotOutput::cleanupFiles(const QStringList &fileNames, QString *error) const
{
    for (const auto &fileName : fileNames) {
        if (!isAllowedWritableName(fileName)) {
            if (error) {
                *error = QStringLiteral("Refusing cleanup of non-allowlisted filename: %1").arg(fileName);
            }
            return false;
        }
        const auto path = QDir(_directory).filePath(fileName);
        const auto info = QFileInfo(path);
        if (!info.exists() && !info.isSymLink()) {
            continue;
        }
        qCInfo(UiScreenshots::lcUiScreenshots) << "Removing stale screenshot output" << fileName;
        if (!QFile::remove(path)) {
            if (error) {
                *error = QStringLiteral("Could not remove stale screenshot output %1.").arg(fileName);
            }
            return false;
        }
    }
    return true;
}

bool UiScreenshotOutput::writeData(const QString &fileName, const QByteArray &data, QString *error) const
{
    if (!isAllowedWritableName(fileName)) {
        if (error) {
            *error = QStringLiteral("Refusing write to non-allowlisted filename: %1").arg(fileName);
        }
        return false;
    }
    if (data.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Refusing to write empty output: %1").arg(fileName);
        }
        return false;
    }

    const auto path = QDir(_directory).filePath(fileName);
    qCInfo(UiScreenshots::lcUiScreenshots) << "Writing screenshot output" << fileName;
    if (!_atomicWriter(path, data, error)) {
        return false;
    }
    if (!verifyFiles({fileName}, error)) {
        return false;
    }
    qCInfo(UiScreenshots::lcUiScreenshots) << "Wrote screenshot output" << fileName;
    return true;
}

bool UiScreenshotOutput::verifyFiles(const QStringList &fileNames, QString *error) const
{
    for (const auto &fileName : fileNames) {
        const auto info = QFileInfo(QDir(_directory).filePath(fileName));
        if (info.isSymLink()) {
            if (error) {
                *error = QStringLiteral("Output is a symbolic link: %1").arg(fileName);
            }
            return false;
        }
        if (!info.isFile() || info.size() <= 0) {
            if (error) {
                *error = QStringLiteral("Output is missing, empty, or not a regular file: %1").arg(fileName);
            }
            return false;
        }
    }
    return true;
}

bool UiScreenshotOutput::verifyMarker(const QString &fileName, const QString &runId, QString *error) const
{
    if (!verifyFiles({fileName}, error)) {
        return false;
    }
    auto marker = QFile(QDir(_directory).filePath(fileName));
    if (!marker.open(QIODevice::ReadOnly) || marker.readAll() != runId.toUtf8()) {
        if (error) {
            *error = QStringLiteral("Completion marker does not match the expected run ID: %1").arg(fileName);
        }
        return false;
    }
    return true;
}

bool UiScreenshotOutput::completeRun(const QStringList &ownedFiles, const QString &markerName, const QString &runId, QString *error) const
{
    if (!isValidRunId(runId)) {
        if (error) {
            *error = QStringLiteral("Cannot complete screenshot phase with an invalid run ID.");
        }
        return false;
    }
    if (!verifyMarker(QString::fromLatin1(runIdMarker), runId, error)
        || !verifyFiles(ownedFiles, error)
        || !writeData(markerName, runId.toUtf8(), error)) {
        return false;
    }
    qCInfo(UiScreenshots::lcUiScreenshots) << "Created completion marker" << markerName;
    return true;
}

bool UiScreenshotOutput::defaultAtomicWrite(const QString &path, const QByteArray &data, QString *error)
{
    auto file = QSaveFile(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) {
            *error = QStringLiteral("Could not open atomic output %1: %2").arg(path, file.errorString());
        }
        return false;
    }
    if (file.write(data) != data.size()) {
        if (error) {
            *error = QStringLiteral("Could not write complete atomic output %1: %2").arg(path, file.errorString());
        }
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        if (error) {
            *error = QStringLiteral("Could not commit atomic output %1: %2").arg(path, file.errorString());
        }
        return false;
    }
    return true;
}

bool UiScreenshotOutput::isAllowedWritableName(const QString &fileName)
{
    if (fileName.isEmpty() || fileName.contains(QLatin1Char('/')) || fileName.contains(QLatin1Char('\\'))) {
        return false;
    }
    return writableFileNames().contains(fileName);
}

}
