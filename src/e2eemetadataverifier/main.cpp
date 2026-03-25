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

#include <cstdio>

#ifdef Q_OS_WIN
#  include <windows.h>
#else
#  include <termios.h>
#  include <unistd.h>
#endif

// For OPENSSL_cleanse — guaranteed secure memory zeroing that is not
// optimized away by the compiler.
#include <openssl/crypto.h>

static QTextStream out(stdout); // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static QTextStream err(stderr); // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

/**
 * Prompt for a passphrase on stderr and read it from stdin without echoing
 * the typed characters.  Falls back to normal (echoing) input when stdin is
 * not connected to a terminal (e.g. piped input in a script).
 *
 * Returns the passphrase as a byte array, stripped of any trailing newline.
 * Returns an empty array when reading fails.
 */
static QByteArray readPassphrase()
{
    err << QStringLiteral("Passphrase: ");
    err.flush();

#ifdef Q_OS_WIN
    const HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    DWORD oldMode = 0;
    const bool isTty = (GetConsoleMode(hStdin, &oldMode) != 0);
    if (isTty) {
        SetConsoleMode(hStdin, oldMode & ~ENABLE_ECHO_INPUT);
    }

    char buf[1024] = {};
    DWORD bytesRead = 0;
    const bool ok = (ReadConsoleA(hStdin, buf, static_cast<DWORD>(sizeof(buf) - 1), &bytesRead, nullptr) != 0);

    if (isTty) {
        SetConsoleMode(hStdin, oldMode);
        err << QLatin1Char('\n');
        err.flush();
    }

    if (!ok) {
        return {};
    }
    QByteArray result(buf, static_cast<int>(bytesRead));
    OPENSSL_cleanse(buf, sizeof(buf));
#else
    const bool isTty = (isatty(STDIN_FILENO) != 0);
    struct termios oldTermios {};
    if (isTty) {
        tcgetattr(STDIN_FILENO, &oldTermios);
        struct termios noEcho = oldTermios;
        noEcho.c_lflag &= ~static_cast<tcflag_t>(ECHO);
        noEcho.c_lflag |= ECHONL; // print a newline after the user presses Enter
        tcsetattr(STDIN_FILENO, TCSANOW, &noEcho);
    }

    char buf[1024] = {};
    const bool ok = (std::fgets(buf, static_cast<int>(sizeof(buf)), stdin) != nullptr);

    if (isTty) {
        tcsetattr(STDIN_FILENO, TCSANOW, &oldTermios);
    }

    if (!ok) {
        return {};
    }
    QByteArray result(buf);
    OPENSSL_cleanse(buf, sizeof(buf));
#endif

    // Strip trailing CR / LF.
    while (!result.isEmpty()
           && (result.back() == '\r' || result.back() == '\n')) {
        result.chop(1);
    }
    return result;
}

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
        QStringLiteral("Path to a PKCS8 or RSA PEM private key file (encrypted or unencrypted).\n"
                       "When provided, the tool decrypts the metadata and prints\n"
                       "the plaintext content in addition to the encrypted structure.\n"
                       "If the key is passphrase-protected you will be prompted to enter\n"
                       "the passphrase on the terminal."),
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

        // When the key is passphrase-protected, prompt for the passphrase
        // interactively before attempting decryption.
        QByteArray passphrase;
        if (OCC::E2EEMetadataDecryptor::isPemEncrypted(privateKeyPem)) {
            passphrase = readPassphrase();
            if (passphrase.isEmpty()) {
                err << QStringLiteral("Error: failed to read passphrase from terminal.\n");
                err.flush();
                return EXIT_FAILURE;
            }
        }

        const auto decryptResult = OCC::E2EEMetadataDecryptor::decrypt(
            root, report.detectedVersion, privateKeyPem, passphrase);

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
