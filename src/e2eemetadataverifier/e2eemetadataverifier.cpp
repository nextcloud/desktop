/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "e2eemetadataverifier.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSslCertificate>

namespace OCC
{

namespace
{

// JSON keys — must match the constants in foldermetadata.cpp
constexpr auto authenticationTagKey = "authenticationTag";
constexpr auto cipherTextKey = "ciphertext";
constexpr auto encryptedKey = "encrypted";
constexpr auto filesKey = "files";
constexpr auto filedropKey = "filedrop";
constexpr auto initializationVectorKey = "initializationVector";
constexpr auto metadataJsonKey = "metadata";
constexpr auto metadataKeyKey = "metadataKey";
constexpr auto nonceKey = "nonce";
constexpr auto usersKey = "users";
constexpr auto usersUserIdKey = "userId";
constexpr auto usersCertificateKey = "certificate";
constexpr auto usersEncryptedMetadataKey = "encryptedMetadataKey";
constexpr auto versionKey = "version";

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Report helpers
// ─────────────────────────────────────────────────────────────────────────────

int E2EEMetadataVerifier::Report::passCount() const
{
    int n = 0;
    for (const auto &c : checks) {
        if (c.result == CheckResult::Pass) {
            ++n;
        }
    }
    return n;
}

int E2EEMetadataVerifier::Report::failCount() const
{
    int n = 0;
    for (const auto &c : checks) {
        if (c.result == CheckResult::Fail) {
            ++n;
        }
    }
    return n;
}

int E2EEMetadataVerifier::Report::warningCount() const
{
    int n = 0;
    for (const auto &c : checks) {
        if (c.result == CheckResult::Warning) {
            ++n;
        }
    }
    return n;
}

bool E2EEMetadataVerifier::Report::isValid() const
{
    return failCount() == 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Private helpers
// ─────────────────────────────────────────────────────────────────────────────

void E2EEMetadataVerifier::addCheck(Report &report, const QString &name, bool passed, const QString &details)
{
    report.checks.append({name, passed ? CheckResult::Pass : CheckResult::Fail, details});
}

void E2EEMetadataVerifier::addWarning(Report &report, const QString &name, const QString &details)
{
    report.checks.append({name, CheckResult::Warning, details});
}

void E2EEMetadataVerifier::addInfo(Report &report, const QString &name, const QString &details)
{
    report.checks.append({name, CheckResult::Info, details});
}

bool E2EEMetadataVerifier::isValidBase64(const QString &str)
{
    if (str.isEmpty()) {
        return false;
    }
    const auto result = QByteArray::fromBase64Encoding(str.toLatin1(), QByteArray::AbortOnBase64EncodingErrors);
    return result.decodingStatus == QByteArray::Base64DecodingStatus::Ok;
}

bool E2EEMetadataVerifier::isValidPipeSeparatedBase64(const QString &str)
{
    // Encrypted blobs are stored as "<base64_cipherdata>|<base64_iv>".
    if (str.isEmpty()) {
        return false;
    }
    const auto parts = str.split(QLatin1Char('|'));
    if (parts.size() != 2) {
        return false;
    }
    return isValidBase64(parts.at(0)) && isValidBase64(parts.at(1));
}

bool E2EEMetadataVerifier::isValidPem(const QString &pem)
{
    const auto certs = QSslCertificate::fromData(pem.toUtf8(), QSsl::Pem);
    return !certs.isEmpty() && !certs.first().isNull();
}

// ─────────────────────────────────────────────────────────────────────────────
// Main entry point
// ─────────────────────────────────────────────────────────────────────────────

E2EEMetadataVerifier::Report E2EEMetadataVerifier::verify(const QByteArray &metadataJson, const QString &filePath)
{
    Report report;
    report.filePath = filePath;

    // 1. Parse JSON
    QJsonParseError parseError;
    const auto doc = QJsonDocument::fromJson(metadataJson, &parseError);
    if (doc.isNull()) {
        addCheck(report,
                 QStringLiteral("JSON document is valid"),
                 false,
                 QStringLiteral("Parse error at offset %1: %2").arg(parseError.offset).arg(parseError.errorString()));
        return report;
    }
    addCheck(report, QStringLiteral("JSON document is valid"), true);

    // 2. Detect and unwrap OCS response envelope if present
    auto root = doc.object();
    if (root.contains(QStringLiteral("ocs"))) {
        const auto metaDataStr = root[QStringLiteral("ocs")]
                                     .toObject()[QStringLiteral("data")]
                                     .toObject()[QStringLiteral("meta-data")]
                                     .toString();
        if (metaDataStr.isEmpty()) {
            addCheck(report,
                     QStringLiteral("OCS envelope: ocs.data.meta-data field present"),
                     false,
                     QStringLiteral("The ocs.data.meta-data key is missing or empty"));
            return report;
        }
        addInfo(report,
                QStringLiteral("Input format"),
                QStringLiteral("OCS API response envelope detected and unwrapped"));

        const auto innerDoc = QJsonDocument::fromJson(metaDataStr.toUtf8(), &parseError);
        if (innerDoc.isNull()) {
            addCheck(report,
                     QStringLiteral("Inner metadata JSON is valid"),
                     false,
                     QStringLiteral("Parse error at offset %1: %2").arg(parseError.offset).arg(parseError.errorString()));
            return report;
        }
        addCheck(report, QStringLiteral("Inner metadata JSON is valid"), true);
        root = innerDoc.object();
    } else {
        addInfo(report,
                QStringLiteral("Input format"),
                QStringLiteral("Direct metadata JSON (no OCS envelope)"));
    }

    // 3. Detect version
    //    V2: "version" is at the root level.
    //    V1: "version" is inside the nested "metadata" object.
    QString versionStr;
    if (root.contains(versionKey)) {
        const auto val = root[versionKey];
        if (val.isString()) {
            versionStr = val.toString();
        } else if (val.isDouble()) {
            versionStr = QString::number(val.toDouble(), 'f', 1);
        }
    }
    if (versionStr.isEmpty() && root.contains(metadataJsonKey)) {
        const auto metaObj = root[metadataJsonKey].toObject();
        if (metaObj.contains(versionKey)) {
            const auto val = metaObj[versionKey];
            if (val.isString()) {
                versionStr = val.toString();
            } else if (val.isDouble()) {
                versionStr = QString::number(val.toDouble(), 'f', 1);
            }
        }
    }

    const bool versionDetected = !versionStr.isEmpty();
    addCheck(report,
             QStringLiteral("Version field present"),
             versionDetected,
             versionDetected ? QStringLiteral("Detected version: \"%1\"").arg(versionStr)
                             : QStringLiteral("No version field found in root or metadata object"));
    report.detectedVersion = versionStr;

    if (!versionDetected) {
        return report;
    }

    // 4. Version-specific structural checks
    if (versionStr == QStringLiteral("1.0") || versionStr == QStringLiteral("1.1") || versionStr == QStringLiteral("1.2")) {
        checkV1(root, report);
    } else if (versionStr == QStringLiteral("2.0") || versionStr == QStringLiteral("2") || versionStr == QStringLiteral("2.1")) {
        checkV2(root, report);
    } else {
        addCheck(report,
                 QStringLiteral("Version recognized"),
                 false,
                 QStringLiteral("Unknown version string: \"%1\"").arg(versionStr));
    }

    return report;
}

// ─────────────────────────────────────────────────────────────────────────────
// V1.x checks (versions 1.0, 1.1 and 1.2)
// ─────────────────────────────────────────────────────────────────────────────

void E2EEMetadataVerifier::checkV1(const QJsonObject &root, Report &report)
{
    const auto &versionStr = report.detectedVersion;
    const bool isV1_2 = (versionStr == QStringLiteral("1.2"));

    // metadata object
    const bool metaPresent = root.contains(metadataJsonKey) && root[metadataJsonKey].isObject();
    addCheck(report, QStringLiteral("\"metadata\" object present"), metaPresent);
    if (!metaPresent) {
        return;
    }
    const auto metaObj = root[metadataJsonKey].toObject();

    // metadataKey
    // V1.1/V1.2 use a single "metadataKey" field.
    // V1.0 used a "metadataKeys" map (before the security-vulnerability fix).
    const auto metadataKeyVal = metaObj[metadataKeyKey].toString();
    if (!metadataKeyVal.isEmpty()) {
        addCheck(report, QStringLiteral("metadata.metadataKey present and non-empty"), true);
        addCheck(report, QStringLiteral("metadata.metadataKey is valid Base64"), isValidBase64(metadataKeyVal));
    } else if (versionStr == QStringLiteral("1.0")) {
        const auto metadataKeysObj = metaObj[QStringLiteral("metadataKeys")].toObject();
        addCheck(report,
                 QStringLiteral("metadata.metadataKeys object present and non-empty (V1.0)"),
                 !metadataKeysObj.isEmpty(),
                 QStringLiteral("%1 key(s)").arg(metadataKeysObj.size()));
        for (auto it = metadataKeysObj.constBegin(); it != metadataKeysObj.constEnd(); ++it) {
            addCheck(report,
                     QStringLiteral("metadata.metadataKeys[\"%1\"] is valid Base64").arg(it.key()),
                     isValidBase64(it.value().toString()));
        }
    } else {
        addCheck(report,
                 QStringLiteral("metadata.metadataKey present and non-empty"),
                 false,
                 QStringLiteral("Field is absent or empty"));
    }

    // checksum (V1.2 only)
    if (isV1_2) {
        const auto checksum = metaObj[QStringLiteral("checksum")].toString();
        addCheck(report,
                 QStringLiteral("metadata.checksum present (V1.2)"),
                 !checksum.isEmpty(),
                 checksum.isEmpty() ? QStringLiteral("Field is absent or empty") : checksum);
    }

    // files object
    if (!root.contains(filesKey)) {
        addInfo(report, QStringLiteral("\"files\" object"), QStringLiteral("Absent — folder appears to be empty"));
    } else {
        addCheck(report, QStringLiteral("\"files\" value is a JSON object"), root[filesKey].isObject());
        if (root[filesKey].isObject()) {
            const auto filesObj = root[filesKey].toObject();
            addInfo(report, QStringLiteral("File entries found"), QString::number(filesObj.size()));

            for (auto it = filesObj.constBegin(); it != filesObj.constEnd(); ++it) {
                const auto entryName = it.key();
                const auto entryObj = it.value().toObject();
                const auto prefix = QStringLiteral("files[\"%1\"]").arg(entryName);

                // encrypted (pipe-separated format: <base64_cipherdata>|<base64_iv>)
                const auto encryptedVal = entryObj[encryptedKey].toString();
                addCheck(report, prefix + QStringLiteral(".encrypted present and non-empty"), !encryptedVal.isEmpty());
                if (!encryptedVal.isEmpty()) {
                    addCheck(report,
                             prefix + QStringLiteral(".encrypted has valid \"<base64>|<base64>\" format"),
                             isValidPipeSeparatedBase64(encryptedVal));
                }

                // initializationVector
                const auto ivVal = entryObj[initializationVectorKey].toString();
                addCheck(report, prefix + QStringLiteral(".initializationVector present"), !ivVal.isEmpty());
                if (!ivVal.isEmpty()) {
                    addCheck(report,
                             prefix + QStringLiteral(".initializationVector is valid Base64"),
                             isValidBase64(ivVal));
                }

                // authenticationTag
                const auto tagVal = entryObj[authenticationTagKey].toString();
                addCheck(report, prefix + QStringLiteral(".authenticationTag present"), !tagVal.isEmpty());
                if (!tagVal.isEmpty()) {
                    addCheck(report,
                             prefix + QStringLiteral(".authenticationTag is valid Base64"),
                             isValidBase64(tagVal));
                }
            }
        }
    }

    // filedrop (optional)
    if (root.contains(filedropKey)) {
        addInfo(report, QStringLiteral("filedrop"), QStringLiteral("filedrop section present"));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// V2.x checks (versions 2.0 and 2.1)
// ─────────────────────────────────────────────────────────────────────────────

void E2EEMetadataVerifier::checkV2(const QJsonObject &root, Report &report)
{
    // metadata object
    const bool metaPresent = root.contains(metadataJsonKey) && root[metadataJsonKey].isObject();
    addCheck(report, QStringLiteral("\"metadata\" object present"), metaPresent);
    if (!metaPresent) {
        return;
    }
    const auto metaObj = root[metadataJsonKey].toObject();

    // ciphertext (pipe-separated: <base64_cipherdata>|<base64_iv>)
    const auto ciphertextVal = metaObj[cipherTextKey].toString();
    addCheck(report, QStringLiteral("metadata.ciphertext present and non-empty"), !ciphertextVal.isEmpty());
    if (!ciphertextVal.isEmpty()) {
        addCheck(report,
                 QStringLiteral("metadata.ciphertext has valid \"<base64>|<base64>\" format"),
                 isValidPipeSeparatedBase64(ciphertextVal));
    }

    // nonce
    const auto nonceVal = metaObj[nonceKey].toString();
    addCheck(report, QStringLiteral("metadata.nonce present"), !nonceVal.isEmpty());
    if (!nonceVal.isEmpty()) {
        addCheck(report, QStringLiteral("metadata.nonce is valid Base64"), isValidBase64(nonceVal));
    }

    // authenticationTag
    const auto tagVal = metaObj[authenticationTagKey].toString();
    addCheck(report, QStringLiteral("metadata.authenticationTag present"), !tagVal.isEmpty());
    if (!tagVal.isEmpty()) {
        addCheck(report, QStringLiteral("metadata.authenticationTag is valid Base64"), isValidBase64(tagVal));
    }

    // users array
    // Present only in root folder metadata; nested folders inherit the key.
    if (!root.contains(usersKey)) {
        addInfo(report,
                QStringLiteral("\"users\" array"),
                QStringLiteral("Absent — this appears to be nested folder metadata "
                               "(encryption key is inherited from the root folder)"));
    } else {
        addCheck(report, QStringLiteral("\"users\" value is a JSON array"), root[usersKey].isArray());
        if (root[usersKey].isArray()) {
            const auto usersArray = root[usersKey].toArray();
            addCheck(report,
                     QStringLiteral("\"users\" array is non-empty"),
                     !usersArray.isEmpty(),
                     QStringLiteral("%1 entry/entries").arg(usersArray.size()));

            for (int i = 0; i < usersArray.size(); ++i) {
                const auto userVal = usersArray.at(i);
                if (!userVal.isObject()) {
                    addCheck(report, QStringLiteral("users[%1] is a JSON object").arg(i), false);
                    continue;
                }
                const auto userObj = userVal.toObject();
                const auto prefix = QStringLiteral("users[%1]").arg(i);

                // userId
                const auto userId = userObj[usersUserIdKey].toString();
                addCheck(report,
                         prefix + QStringLiteral(".userId present and non-empty"),
                         !userId.isEmpty(),
                         userId.isEmpty() ? QString{} : userId);

                // certificate
                const auto certPem = userObj[usersCertificateKey].toString();
                addCheck(report, prefix + QStringLiteral(".certificate present"), !certPem.isEmpty());
                if (!certPem.isEmpty()) {
                    addCheck(report,
                             prefix + QStringLiteral(".certificate is a valid PEM X.509 certificate"),
                             isValidPem(certPem));
                }

                // encryptedMetadataKey
                const auto encMetaKey = userObj[usersEncryptedMetadataKey].toString();
                addCheck(report, prefix + QStringLiteral(".encryptedMetadataKey present"), !encMetaKey.isEmpty());
                if (!encMetaKey.isEmpty()) {
                    addCheck(report,
                             prefix + QStringLiteral(".encryptedMetadataKey is valid Base64"),
                             isValidBase64(encMetaKey));
                }
            }
        }
    }

    // filedrop (optional)
    if (root.contains(filedropKey)) {
        addInfo(report, QStringLiteral("filedrop"), QStringLiteral("filedrop section present"));
    }
}

} // namespace OCC
