/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: CC0-1.0
 */

#include "uiscreenshots/uiscreenshotoutput.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>
#include <QUuid>

using namespace OCC;

class UiScreenshotOutputTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        Q_INIT_RESOURCE(resources);
        Q_INIT_RESOURCE(theme);
    }

    void validatesOnlyTheFixedStagingDirectory()
    {
        auto temporaryRoot = QTemporaryDir{};
        QVERIFY(temporaryRoot.isValid());

        const auto expectedRoot = temporaryRoot.path();
        const auto stagingDirectory = QDir(expectedRoot).filePath(QStringLiteral("nextcloud-ui-screenshots"));
        auto error = QString{};

        QVERIFY(UiScreenshotOutput::validateStagingPath(stagingDirectory, expectedRoot, &error));
        QVERIFY(error.isEmpty());
        QVERIFY(!UiScreenshotOutput::validateStagingPath(QDir(expectedRoot).filePath(QStringLiteral("other")), expectedRoot, &error));
        QVERIFY(!UiScreenshotOutput::validateStagingPath(QStringLiteral("${HOME}/screenshots"), expectedRoot, &error));
        QVERIFY(!UiScreenshotOutput::validateStagingPath(stagingDirectory, QStringLiteral("/"), &error));
    }

    void completesBothPhasesWithOneRunId()
    {
        auto temporaryRoot = QTemporaryDir{};
        QVERIFY(temporaryRoot.isValid());

        const auto stagingDirectory = QDir(temporaryRoot.path()).filePath(QStringLiteral("nextcloud-ui-screenshots"));
        auto output = UiScreenshotOutput(stagingDirectory, temporaryRoot.path());
        auto runId = QString{};
        auto error = QString{};

        QVERIFY2(output.beginQmlRun(&runId, &error), qPrintable(error));
        QVERIFY(UiScreenshotOutput::isValidRunId(runId));

        auto image = QImage(2, 2, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        for (const auto &fileName : UiScreenshotOutput::qmlPngFiles()) {
            QVERIFY2(output.writePng(fileName, image, &error), qPrintable(error));
        }
        QVERIFY2(output.exportStatusSvgs(&error), qPrintable(error));
        for (const auto &[resourceName, outputName] : UiScreenshots::svgResourceMappings()) {
            auto resource = QFile(resourceName);
            auto exported = QFile(QDir(stagingDirectory).filePath(outputName));
            if (!resource.open(QIODevice::ReadOnly)) {
                QFAIL(qPrintable(resource.errorString()));
            }
            if (!exported.open(QIODevice::ReadOnly)) {
                QFAIL(qPrintable(exported.errorString()));
            }
            QCOMPARE_EQ(exported.readAll(), resource.readAll());
        }
        QVERIFY2(output.completeQmlRun(runId, &error), qPrintable(error));

        const auto otherRunId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        QVERIFY(!output.beginNativeRun(otherRunId, &error));
        QVERIFY2(output.beginNativeRun(runId, &error), qPrintable(error));
        for (const auto &fileName : UiScreenshotOutput::nativePngFiles()) {
            QVERIFY2(output.writePng(fileName, image, &error), qPrintable(error));
        }
        QVERIFY2(output.completeNativeRun(runId, &error), qPrintable(error));

        for (const auto &fileName : UiScreenshotOutput::deliverableFiles()) {
            const auto info = QFileInfo(QDir(stagingDirectory).filePath(fileName));
            QVERIFY(info.isFile());
            QVERIFY(info.size() > 0);
        }
    }

    void propagatesAtomicWriterFailure()
    {
        auto temporaryRoot = QTemporaryDir{};
        QVERIFY(temporaryRoot.isValid());

        const auto stagingDirectory = QDir(temporaryRoot.path()).filePath(QStringLiteral("nextcloud-ui-screenshots"));
        const auto failingWriter = [](const QString &, const QByteArray &, QString *error) {
            if (error) {
                *error = QStringLiteral("Injected write failure.");
            }
            return false;
        };
        auto output = UiScreenshotOutput(stagingDirectory, temporaryRoot.path(), failingWriter);
        auto runId = QString{};
        auto error = QString{};

        QVERIFY(!output.beginQmlRun(&runId, &error));
        QCOMPARE_EQ(error, QStringLiteral("Injected write failure."));
        QVERIFY(runId.isEmpty());
    }
};

QTEST_GUILESS_MAIN(UiScreenshotOutputTest)

#include "UiScreenshotOutputTest.moc"
