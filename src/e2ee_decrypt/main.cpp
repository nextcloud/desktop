/*
 * SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "e2eedecryptor.h"

#include <QCoreApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QLoggingCategory>
#include <QString>

Q_LOGGING_CATEGORY(lcE2eeDecryptMain, "nextcloud.e2ee_decrypt.main", QtInfoMsg)

int main(int argc, char *argv[])
{
    auto app = QCoreApplication{argc, argv};
    QCoreApplication::setApplicationName(QStringLiteral("nextcloud-e2ee-decrypt"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0"));

    auto parser = QCommandLineParser{};
    parser.setApplicationDescription(
        QStringLiteral("Offline decryption of Nextcloud end-to-end encrypted folders (V2+ metadata only)."));
    parser.addHelpOption();
    parser.addVersionOption();

    const auto metadataOption = QCommandLineOption{
        {QStringLiteral("m"), QStringLiteral("metadata")},
        QStringLiteral("Path to the folder metadata JSON file (OCS-wrapped or raw inner JSON)."),
        QStringLiteral("path")};

    const auto privateKeyOption = QCommandLineOption{
        {QStringLiteral("k"), QStringLiteral("private-key")},
        QStringLiteral("Path to the encrypted private key file "
                       "(format: base64(ct+tag)|base64(iv)|base64(salt))."),
        QStringLiteral("path")};

    const auto certificateOption = QCommandLineOption{
        {QStringLiteral("c"), QStringLiteral("certificate")},
        QStringLiteral("Path to the user certificate PEM file."),
        QStringLiteral("path")};

    const auto passphraseOption = QCommandLineOption{
        {QStringLiteral("p"), QStringLiteral("passphrase")},
        QStringLiteral("Mnemonic or passphrase used to decrypt the private key. "
                       "Spaces are allowed; they are stripped internally."),
        QStringLiteral("passphrase")};

    const auto encryptedDirOption = QCommandLineOption{
        {QStringLiteral("i"), QStringLiteral("encrypted-dir")},
        QStringLiteral("Directory containing the encrypted source files."),
        QStringLiteral("path")};

    const auto outputDirOption = QCommandLineOption{
        {QStringLiteral("o"), QStringLiteral("output-dir")},
        QStringLiteral("Directory to write the decrypted output files."),
        QStringLiteral("path")};

    const auto userIdOption = QCommandLineOption{
        QStringLiteral("user-id"),
        QStringLiteral("UserId to look up in the metadata 'users' array. "
                       "Auto-detected from the certificate CN if omitted."),
        QStringLiteral("userId")};

    const auto parentMetadataOption = QCommandLineOption{
        QStringLiteral("parent-metadata"),
        QStringLiteral("Path to the root folder metadata file. "
                       "Required when decrypting a nested (non-root) encrypted folder "
                       "whose metadata lacks a 'users' array."),
        QStringLiteral("path")};

    parser.addOption(metadataOption);
    parser.addOption(privateKeyOption);
    parser.addOption(certificateOption);
    parser.addOption(passphraseOption);
    parser.addOption(encryptedDirOption);
    parser.addOption(outputDirOption);
    parser.addOption(userIdOption);
    parser.addOption(parentMetadataOption);

    parser.process(app);

    // Validate required options
    auto missingOptions = QStringList{};
    const auto requireOption = [&](const QCommandLineOption &opt) {
        if (!parser.isSet(opt)) {
            missingOptions << QStringLiteral("--") + opt.names().last();
        }
    };
    requireOption(metadataOption);
    requireOption(privateKeyOption);
    requireOption(certificateOption);
    requireOption(passphraseOption);
    requireOption(encryptedDirOption);
    requireOption(outputDirOption);

    if (!missingOptions.isEmpty()) {
        qCCritical(lcE2eeDecryptMain) << "Missing required options:" << missingOptions.join(QStringLiteral(", "));
        parser.showHelp(1);
    }

    const auto options = OCC::E2eeDecryptor::Options{
        .metadataPath = parser.value(metadataOption),
        .privateKeyPath = parser.value(privateKeyOption),
        .certificatePath = parser.value(certificateOption),
        .passphrase = parser.value(passphraseOption),
        .userId = parser.value(userIdOption),
        .parentMetadataPath = parser.value(parentMetadataOption),
        .encryptedDir = parser.value(encryptedDirOption),
        .outputDir = parser.value(outputDirOption),
    };

    auto decryptor = OCC::E2eeDecryptor{options};
    return decryptor.run();
}
