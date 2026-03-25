/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "e2eemetadatadecryptor.h"
#include "e2eemetadataverifier.h"

#include <QCommandLineOption>
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
        QStringLiteral("Reads a Nextcloud E2EE folder metadata file, verifies its structural\n"
                       "integrity, and optionally decrypts its content using the folder owner's\n"
                       "private key.\n\n"
                       "Pass '-' as the file argument to read from standard input."));
    parser.addHelpOption();
    parser.addVersionOption();

    const QCommandLineOption privateKeyOption(
        {QStringLiteral("k"), QStringLiteral("private-key")},
        QStringLiteral("Path to an unencrypted PKCS8 or RSA PEM private key file.\n"
                       "When provided, the tool decrypts the metadata and prints\n"
                       "the plaintext content in addition to the encrypted structure."),
        QStringLiteral("file"));
    parser.addOption(privateKeyOption);

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

    // Read the metadata file (or stdin).
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

    // Print the encrypted metadata structure.
    out << QStringLiteral("=== Encrypted metadata ===\n");
    out << QJsonDocument(root).toJson(QJsonDocument::Indented);
    out.flush();

    // Verify structural integrity and collect format errors.
    const auto report = OCC::E2EEMetadataVerifier::verify(metadataJson, filePath);

    // Optionally decrypt using the provided private key.
    bool decryptionFailed = false;
    if (parser.isSet(privateKeyOption)) {
        const auto privateKeyPath = parser.value(privateKeyOption);
        QFile keyFile(privateKeyPath);
        if (!keyFile.open(QIODevice::ReadOnly)) {
            err << QStringLiteral("Error: cannot open private key file \"%1\": %2\n")
                       .arg(privateKeyPath, keyFile.errorString());
            err.flush();
            return EXIT_FAILURE;
        }
        const auto privateKeyPem = keyFile.readAll();
        if (privateKeyPem.isEmpty()) {
            err << QStringLiteral("Error: private key file \"%1\" is empty.\n").arg(privateKeyPath);
            err.flush();
            return EXIT_FAILURE;
        }

        const auto decryptResult = OCC::E2EEMetadataDecryptor::decrypt(
            root, report.detectedVersion, privateKeyPem);

        if (decryptResult.success) {
            out << QStringLiteral("\n=== Decrypted content ===\n");
            out << QJsonDocument(decryptResult.decryptedContent).toJson(QJsonDocument::Indented);
            out.flush();
        } else {
            err << QStringLiteral("\nDecryption error: ") << decryptResult.error << QLatin1Char('\n');
            err.flush();
            decryptionFailed = true;
        }
    }

    // Report any format failures.
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
    }

    if (decryptionFailed || !report.isValid()) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
