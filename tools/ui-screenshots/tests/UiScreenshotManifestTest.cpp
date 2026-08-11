/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: CC0-1.0
 */

#include "uiscreenshots/uiscreenshotmanifest.h"
#include "uiscreenshots/uiscreenshotoutput.h"

#include <QDir>
#include <QFile>
#include <QSet>
#include <QTest>

using namespace OCC;

namespace {
QSet<QString> toSet(const QStringList &values)
{
    auto result = QSet<QString>{};
    for (const auto &value : values) {
        result.insert(value);
    }
    return result;
}
}

class UiScreenshotManifestTest : public QObject
{
    Q_OBJECT

private slots:
    void derivesPngAllowlistsFromTypedJobs()
    {
        auto qmlNames = QStringList{};
        for (const auto &job : UiScreenshots::qmlScreenshotJobs()) {
            QVERIFY(!job.outputName.isEmpty());
            QVERIFY(job.componentUrl.isValid());
            qmlNames.append(job.outputName);
        }

        auto nativeNames = QStringList{};
        for (const auto &job : UiScreenshots::nativeScreenshotJobs()) {
            QVERIFY(!job.outputName.isEmpty());
            QCOMPARE_EQ(UiScreenshots::nativeScreenshotOutputName(job.kind), job.outputName);
            nativeNames.append(job.outputName);
        }

        QCOMPARE_EQ(qmlNames.size(), 6);
        QCOMPARE_EQ(nativeNames.size(), 6);
        QCOMPARE_EQ(toSet(qmlNames).size(), qmlNames.size());
        QCOMPARE_EQ(toSet(nativeNames).size(), nativeNames.size());
        QCOMPARE_EQ(UiScreenshotOutput::qmlPngFiles(), qmlNames);
        QCOMPARE_EQ(UiScreenshotOutput::nativePngFiles(), nativeNames);
        QVERIFY(UiScreenshots::nativeScreenshotOutputName(UiScreenshots::NativeScreenshotJobKind::Finished).isEmpty());
    }

    void keepsDeliverablesUniqueAndDisjoint()
    {
        const auto deliverables = UiScreenshotOutput::deliverableFiles();
        const auto deliverableSet = toSet(deliverables);

        QCOMPARE_EQ(deliverables.size(), 24);
        QCOMPARE_EQ(deliverableSet.size(), deliverables.size());

        for (const auto &fileName : UiScreenshots::excludedPngFileNames()) {
            QVERIFY(!deliverableSet.contains(fileName));
        }
        for (const auto &fileName : UiScreenshots::whiteSvgFileNames()) {
            QVERIFY(!deliverableSet.contains(fileName));
        }
    }

    void keepsSvgMappingsUnique()
    {
        auto resources = QSet<QString>{};
        auto outputs = QSet<QString>{};
        for (const auto &[resourceName, outputName] : UiScreenshots::svgResourceMappings()) {
            QVERIFY(!resourceName.isEmpty());
            QVERIFY(!outputName.isEmpty());
            QVERIFY(!resources.contains(resourceName));
            QVERIFY(!outputs.contains(outputName));
            resources.insert(resourceName);
            outputs.insert(outputName);
        }

        QCOMPARE_EQ(outputs.size(), 12);
        const auto svgFiles = UiScreenshotOutput::svgFiles();
        QCOMPARE_EQ(toSet(svgFiles), outputs);
    }

    void usesOneAuthoritativeManifest()
    {
        auto manifest = QFile(QDir(QStringLiteral(UI_SCREENSHOT_SOURCE_DIR)).filePath(QStringLiteral("manifest.tsv")));
        if (!manifest.open(QIODevice::ReadOnly)) {
            QFAIL(qPrintable(manifest.errorString()));
        }
        auto manifestQmlOutputs = QStringList{};
        auto manifestQmlSources = QStringList{};
        auto manifestNativeOutputs = QStringList{};
        auto manifestSvgOutputs = QStringList{};
        auto manifestSvgSources = QStringList{};
        auto manifestExcludedPngs = QStringList{};
        auto manifestWhiteSvgs = QStringList{};
        const auto lines = QString::fromUtf8(manifest.readAll()).split(QLatin1Char('\n'));
        for (const auto &rawLine : lines) {
            const auto line = rawLine.trimmed();
            if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
                continue;
            }
            const auto fields = line.split(QLatin1Char('\t'));
            QCOMPARE_EQ(fields.size(), 4);
            const auto &category = fields.at(0);
            if (category == QStringLiteral("qml")) {
                manifestQmlOutputs.append(fields.at(2));
                manifestQmlSources.append(fields.at(3));
            } else if (category == QStringLiteral("native")) {
                manifestNativeOutputs.append(fields.at(2));
            } else if (category == QStringLiteral("svg")) {
                manifestSvgOutputs.append(fields.at(2));
                manifestSvgSources.append(fields.at(3));
            } else if (category == QStringLiteral("excluded-png")) {
                manifestExcludedPngs.append(fields.at(2));
            } else if (category == QStringLiteral("white-svg")) {
                manifestWhiteSvgs.append(fields.at(2));
            } else {
                QFAIL(qPrintable(QStringLiteral("Unexpected manifest category: %1").arg(category)));
            }
        }

        auto qmlOutputs = QStringList{};
        auto qmlSources = QStringList{};
        for (const auto &job : UiScreenshots::qmlScreenshotJobs()) {
            qmlOutputs.append(job.outputName);
            qmlSources.append(job.componentUrl.toString());
            QVERIFY(job.componentUrl.toString().startsWith(QStringLiteral("qrc:/qml/src/gui/")));
        }
        auto nativeOutputs = QStringList{};
        for (const auto &job : UiScreenshots::nativeScreenshotJobs()) {
            nativeOutputs.append(job.outputName);
        }
        auto svgOutputs = QStringList{};
        auto svgSources = QStringList{};
        for (const auto &[resourceName, outputName] : UiScreenshots::svgResourceMappings()) {
            svgOutputs.append(outputName);
            svgSources.append(resourceName);
        }

        QCOMPARE_EQ(qmlOutputs, manifestQmlOutputs);
        QCOMPARE_EQ(qmlSources, manifestQmlSources);
        QCOMPARE_EQ(nativeOutputs, manifestNativeOutputs);
        QCOMPARE_EQ(svgOutputs, manifestSvgOutputs);
        QCOMPARE_EQ(svgSources, manifestSvgSources);
        QCOMPARE_EQ(UiScreenshots::excludedPngFileNames(), manifestExcludedPngs);
        QCOMPARE_EQ(UiScreenshots::whiteSvgFileNames(), manifestWhiteSvgs);

        auto shellManifest = QFile(QDir(QStringLiteral(UI_SCREENSHOT_SOURCE_DIR)).filePath(QStringLiteral("manifest.sh")));
        if (!shellManifest.open(QIODevice::ReadOnly)) {
            QFAIL(qPrintable(shellManifest.errorString()));
        }
        const auto shellContents = shellManifest.readAll();
        QVERIFY(shellContents.contains("manifest.tsv"));
        for (const auto &fileName : UiScreenshotOutput::deliverableFiles()) {
            QVERIFY2(!shellContents.contains(fileName.toUtf8()), qPrintable(fileName));
        }
    }

    void keepsVisualConstructionInProduction()
    {
        const auto sourceDirectory = QDir(QStringLiteral(UI_SCREENSHOT_SOURCE_DIR));

        auto nativeCapture = QFile(sourceDirectory.filePath(QStringLiteral("uiscreenshots/nativescreenshotcaptureutils.cpp")));
        if (!nativeCapture.open(QIODevice::ReadOnly)) {
            QFAIL(qPrintable(nativeCapture.errorString()));
        }
        const auto nativeSource = nativeCapture.readAll();
        QVERIFY(nativeSource.contains("showConnectionSettingsDialog"));
        QVERIFY(nativeSource.contains("slotIgnoreFilesEditor"));
        QVERIFY(!nativeSource.contains("new QDialog"));
        QVERIFY(!nativeSource.contains("new NetworkSettings"));
        QVERIFY(!nativeSource.contains("new IgnoreListEditor"));

        auto qmlCapture = QFile(sourceDirectory.filePath(QStringLiteral("uiscreenshots/qmlscreenshotcaptureutils.cpp")));
        if (!qmlCapture.open(QIODevice::ReadOnly)) {
            QFAIL(qPrintable(qmlCapture.errorString()));
        }
        const auto qmlSource = qmlCapture.readAll();
        QVERIFY(qmlSource.contains("styleNativeTitleBar"));
        QVERIFY(!qmlSource.contains("ScreenshotQuickWindowDragHandle"));
    }

    void keepsScreenshotCompilationOutOfNormalDeveloperTargets()
    {
        const auto sourceDirectory = QDir(QStringLiteral(UI_SCREENSHOT_SOURCE_DIR));
        const auto projectDirectory = QDir(sourceDirectory.absoluteFilePath(QStringLiteral("../..")));

        auto guiCMake = QFile(projectDirectory.filePath(QStringLiteral("src/gui/CMakeLists.txt")));
        if (!guiCMake.open(QIODevice::ReadOnly)) {
            QFAIL(qPrintable(guiCMake.errorString()));
        }
        const auto guiCMakeSource = guiCMake.readAll();
        QVERIFY(!guiCMakeSource.contains("tools/ui-screenshots"));
        QVERIFY(!guiCMakeSource.contains("NEXTCLOUD_UI_SCREENSHOT_ISOLATED_BUILD"));

        auto clientMain = QFile(projectDirectory.filePath(QStringLiteral("src/gui/main.cpp")));
        if (!clientMain.open(QIODevice::ReadOnly)) {
            QFAIL(qPrintable(clientMain.errorString()));
        }
        const auto clientMainSource = clientMain.readAll();
        QVERIFY(!clientMainSource.contains("uiscreenshot"));
        QVERIFY(!clientMainSource.contains("NEXTCLOUD_UI_SCREENSHOT"));

        auto fileProviderController = QFile(projectDirectory.filePath(
            QStringLiteral("src/gui/macOS/fileprovidersettingscontroller_mac.mm")));
        if (!fileProviderController.open(QIODevice::ReadOnly)) {
            QFAIL(qPrintable(fileProviderController.errorString()));
        }
        QVERIFY(!fileProviderController.readAll().contains("NEXTCLOUD_UI_SCREENSHOT"));

        auto screenshotCMake = QFile(sourceDirectory.filePath(QStringLiteral("CMakeLists.txt")));
        if (!screenshotCMake.open(QIODevice::ReadOnly)) {
            QFAIL(qPrintable(screenshotCMake.errorString()));
        }
        const auto screenshotCMakeSource = screenshotCMake.readAll();
        QVERIFY(screenshotCMakeSource.contains("add_executable(nextcloud-ui-screenshots"));
        QVERIFY(screenshotCMakeSource.contains("NEXTCLOUD_UI_SCREENSHOT_ISOLATED_CMAKE_BUILD"));
        QVERIFY(screenshotCMakeSource.contains("fileprovidersettingscontrollerfixture.cpp"));
        QVERIFY(screenshotCMakeSource.contains("set_property(TARGET nextcloudCore PROPERTY SOURCES"));
        QVERIFY(!screenshotCMakeSource.contains("target_sources(nextcloudCore"));
        QVERIFY(!screenshotCMakeSource.contains("target_link_libraries(nextcloud PRIVATE"));

        auto buildScript = QFile(sourceDirectory.filePath(QStringLiteral("build-screenshot-tool.sh")));
        if (!buildScript.open(QIODevice::ReadOnly)) {
            QFAIL(qPrintable(buildScript.errorString()));
        }
        const auto buildScriptSource = buildScript.readAll();
        QVERIFY(buildScriptSource.contains("CMAKE_PROJECT_TOP_LEVEL_INCLUDES"));
        QVERIFY(buildScriptSource.contains("build-paths.sh"));
        QVERIFY(buildScriptSource.contains("--target nextcloud-ui-screenshots"));

        auto injection = QFile(sourceDirectory.filePath(QStringLiteral("inject.cmake")));
        if (!injection.open(QIODevice::ReadOnly)) {
            QFAIL(qPrintable(injection.errorString()));
        }
        const auto injectionSource = injection.readAll();
        QVERIFY(injectionSource.contains("cmake_language(DEFER"));
        QVERIFY(injectionSource.contains("CALL include"));
        QVERIFY(!injectionSource.contains("\nadd_subdirectory("));

        auto buildPaths = QFile(sourceDirectory.filePath(QStringLiteral("build-paths.sh")));
        if (!buildPaths.open(QIODevice::ReadOnly)) {
            QFAIL(qPrintable(buildPaths.errorString()));
        }
        QVERIFY(buildPaths.readAll().contains("build/ui-screenshots-"));
    }
};

QTEST_GUILESS_MAIN(UiScreenshotManifestTest)

#include "UiScreenshotManifestTest.moc"
