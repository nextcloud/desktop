/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>

namespace OCC
{

/**
 * @brief Verifier for Nextcloud E2EE folder metadata files.
 *
 * Inspects the structural integrity of Nextcloud end-to-end encrypted folder
 * metadata without requiring a live account or decryption keys.
 *
 * Concretely it validates:
 *   - JSON document validity and OCS-envelope unwrapping
 *   - Presence and correct types of required fields for each metadata version
 *   - Base64 encoding of all binary blobs (keys, nonces, authentication tags)
 *   - The pipe-separated "cipherdata|iv" format used for encrypted payloads
 *   - X.509 certificate PEM data stored in the users array (V2 only)
 *
 * Supported metadata versions: 1.0, 1.1, 1.2, 2.0 and 2.1.
 * See https://github.com/nextcloud/end_to_end_encryption_rfc for the spec.
 */
class E2EEMetadataVerifier
{
public:
    /// Result of a single verification check.
    enum class CheckResult {
        Pass,    ///< Check passed successfully.
        Fail,    ///< Check failed; the metadata is structurally invalid.
        Warning, ///< Non-fatal issue; metadata may still be functional.
        Info,    ///< Informational note, not a pass/fail test.
    };

    struct Check {
        QString name;
        CheckResult result = CheckResult::Pass;
        QString details;
    };

    struct Report {
        QString filePath;
        QString detectedVersion;
        QVector<Check> checks;

        [[nodiscard]] int passCount() const;
        [[nodiscard]] int failCount() const;
        [[nodiscard]] int warningCount() const;
        [[nodiscard]] bool isValid() const;
    };

    /**
     * Verify the E2EE metadata contained in @p metadataJson.
     *
     * The input may be either a raw metadata JSON object or an OCS API
     * response envelope ({"ocs":{"data":{"meta-data":"..."}}}).
     *
     * @param metadataJson  Raw bytes of the JSON to inspect.
     * @param filePath      Optional file path shown in the report.
     * @return              A Report with the outcome of every check.
     */
    [[nodiscard]] static Report verify(const QByteArray &metadataJson, const QString &filePath = {});

private:
    static void checkV1(const QJsonObject &root, Report &report);
    static void checkV2(const QJsonObject &root, Report &report);

    static void addCheck(Report &report, const QString &name, bool passed, const QString &details = {});
    static void addWarning(Report &report, const QString &name, const QString &details = {});
    static void addInfo(Report &report, const QString &name, const QString &details = {});

    static bool isValidBase64(const QString &str);
    static bool isValidPipeSeparatedBase64(const QString &str);
    static bool isValidPem(const QString &pem);
};

} // namespace OCC
