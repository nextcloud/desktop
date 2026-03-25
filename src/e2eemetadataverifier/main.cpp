/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "e2eemetadataverifier.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFile>
#include <QTextStream>

static QTextStream out(stdout); // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static QTextStream err(stderr); // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static QString checkResultSymbol(OCC::E2EEMetadataVerifier::CheckResult result)
{
    switch (result) {
    case OCC::E2EEMetadataVerifier::CheckResult::Pass:
        return QStringLiteral("[PASS]");
    case OCC::E2EEMetadataVerifier::CheckResult::Fail:
        return QStringLiteral("[FAIL]");
    case OCC::E2EEMetadataVerifier::CheckResult::Warning:
        return QStringLiteral("[WARN]");
    case OCC::E2EEMetadataVerifier::CheckResult::Info:
        return QStringLiteral("[INFO]");
    }
    return {};
}

static void printReport(const OCC::E2EEMetadataVerifier::Report &report, bool quietMode)
{
    out << QStringLiteral("Nextcloud End-to-End Encryption Metadata Verifier\n");
    out << QStringLiteral("==================================================\n");
    if (!report.filePath.isEmpty()) {
        out << QStringLiteral("File    : ") << report.filePath << QLatin1Char('\n');
    }
    if (!report.detectedVersion.isEmpty()) {
        out << QStringLiteral("Version : ") << report.detectedVersion << QLatin1Char('\n');
    }
    out << QLatin1Char('\n');

    for (const auto &check : report.checks) {
        if (quietMode && check.result == OCC::E2EEMetadataVerifier::CheckResult::Pass) {
            continue;
        }
        if (quietMode && check.result == OCC::E2EEMetadataVerifier::CheckResult::Info) {
            continue;
        }
        out << QStringLiteral("  ") << checkResultSymbol(check.result) << QLatin1Char(' ') << check.name;
        if (!check.details.isEmpty()) {
            out << QStringLiteral(": ") << check.details;
        }
        out << QLatin1Char('\n');
    }

    out << QLatin1Char('\n');
    const int totalChecks = report.passCount() + report.failCount() + report.warningCount();
    out << QStringLiteral("Summary: %1/%2 checks passed").arg(report.passCount()).arg(totalChecks);
    if (report.warningCount() > 0) {
        out << QStringLiteral(", %1 warning(s)").arg(report.warningCount());
    }
    out << QStringLiteral(" \xe2\x80\x94 ") // em dash
        << (report.isValid() ? QStringLiteral("VALID") : QStringLiteral("INVALID")) << QLatin1Char('\n');
    out.flush();
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral(APPLICATION_EXECUTABLE "e2eemetadataverifier"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Verifies the structural integrity of a Nextcloud end-to-end encrypted "
                       "folder metadata file without requiring a live account or decryption "
                       "keys.\n\n"
                       "Pass '-' as the file argument to read from standard input."));
    parser.addHelpOption();
    parser.addVersionOption();

    const QCommandLineOption quietOption(
        {QStringLiteral("q"), QStringLiteral("quiet")},
        QStringLiteral("Only show failures and warnings; suppress passed checks and informational notes."));
    parser.addOption(quietOption);

    parser.addPositionalArgument(
        QStringLiteral("file"),
        QStringLiteral("Metadata JSON file to verify, or '-' to read from stdin."));

    parser.process(app);

    const auto positional = parser.positionalArguments();
    if (positional.isEmpty()) {
        err << QStringLiteral("Error: no input file specified.\n");
        err << parser.helpText();
        err.flush();
        return EXIT_FAILURE;
    }

    const bool quietMode = parser.isSet(quietOption);
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

    const auto report = OCC::E2EEMetadataVerifier::verify(metadataJson, filePath);
    printReport(report, quietMode);

    return report.isValid() ? EXIT_SUCCESS : EXIT_FAILURE;
}
