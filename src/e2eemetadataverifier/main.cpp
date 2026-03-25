/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "e2eemetadataverifier.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

static QTextStream out(stdout); // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static QTextStream err(stderr); // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral(APPLICATION_EXECUTABLE "e2eemetadataverifier"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Reads a Nextcloud E2EE folder metadata file, extracts its content, "
                       "and verifies its structural integrity.\n\n"
                       "Pass '-' as the file argument to read from standard input."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(
        QStringLiteral("file"),
        QStringLiteral("Metadata JSON file to read, or '-' to read from stdin."));
    parser.process(app);

    const auto positional = parser.positionalArguments();
    if (positional.isEmpty()) {
        err << QStringLiteral("Error: no input file specified.\n");
        err << parser.helpText();
        err.flush();
        return EXIT_FAILURE;
    }

    const auto filePath = positional.first();

    QByteArray metadataJson;
    if (filePath == QStringLiteral("-")) {
        QFile stdinFile;
        stdinFile.open(stdin, QIODevice::ReadOnly);
        metadataJson = stdinFile.readAll();
    } else {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            err << QStringLiteral("Error: cannot open \"%1\": %2\n").arg(filePath, file.errorString());
            err.flush();
            return EXIT_FAILURE;
        }
        metadataJson = file.readAll();
    }

    if (metadataJson.isEmpty()) {
        err << QStringLiteral("Error: input is empty.\n");
        err.flush();
        return EXIT_FAILURE;
    }

    // Parse the outer JSON and unwrap the OCS API envelope when present so
    // that the printed content is always the bare metadata object.
    QJsonParseError parseError;
    auto doc = QJsonDocument::fromJson(metadataJson, &parseError);
    if (doc.isNull()) {
        err << QStringLiteral("Error: invalid JSON: %1\n").arg(parseError.errorString());
        err.flush();
        return EXIT_FAILURE;
    }

    auto root = doc.object();
    if (root.contains(QStringLiteral("ocs"))) {
        const auto metaDataStr = root[QStringLiteral("ocs")]
                                     .toObject()[QStringLiteral("data")]
                                     .toObject()[QStringLiteral("meta-data")]
                                     .toString();
        const auto innerDoc = QJsonDocument::fromJson(metaDataStr.toUtf8(), &parseError);
        if (!innerDoc.isNull()) {
            root = innerDoc.object();
        }
    }

    // Print the extracted metadata content in human-readable form.
    out << QJsonDocument(root).toJson(QJsonDocument::Indented);
    out.flush();

    // Verify the structural integrity of the metadata and report any failures.
    const auto report = OCC::E2EEMetadataVerifier::verify(metadataJson, filePath);
    if (!report.isValid()) {
        err << QStringLiteral("\nFormat errors:\n");
        for (const auto &check : report.checks) {
            if (check.result != OCC::E2EEMetadataVerifier::CheckResult::Fail) {
                continue;
            }
            err << QStringLiteral("  [FAIL] ") << check.name;
            if (!check.details.isEmpty()) {
                err << QStringLiteral(": ") << check.details;
            }
            err << QLatin1Char('\n');
        }
        err.flush();
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
