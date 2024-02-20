/*
 * Copyright (C) 2023 by Oleksandr Zolotov <alex@nextcloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include "account.h"
#include "encryptedfoldermetadatahandler.h"
#include "foldermetadata.h"
#include "clientsideencryption.h"
#include "clientsideencryptionjobs.h"
#include <common/checksums.h>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSslCertificate>

namespace OCC
{
Q_LOGGING_CATEGORY(lcCseMetadata, "nextcloud.metadata", QtInfoMsg)

namespace
{
constexpr auto authenticationTagKey = "authenticationTag";
constexpr auto cipherTextKey = "ciphertext";
constexpr auto counterKey = "counter";
constexpr auto filesKey = "files";
constexpr auto filedropKey = "filedrop";
constexpr auto foldersKey = "folders";
constexpr auto initializationVectorKey = "initializationVector";
constexpr auto keyChecksumsKey = "keyChecksums";
constexpr auto metadataJsonKey = "metadata";
constexpr auto metadataKeyKey = "metadataKey";
constexpr auto nonceKey = "nonce";
constexpr auto usersKey = "users";
constexpr auto usersUserIdKey = "userId";
constexpr auto usersCertificateKey = "certificate";
constexpr auto usersEncryptedMetadataKey = "encryptedMetadataKey";
constexpr auto usersEncryptedFiledropKey = "encryptedFiledropKey";
constexpr auto versionKey = "version";
constexpr auto encryptedKey = "encrypted";

const auto metadataKeySize = 16;

QString metadataStringFromOCsDocument(const QJsonDocument &ocsDoc)
{
    return ocsDoc.object()["ocs"].toObject()["data"].toObject()["meta-data"].toString();
}
}

bool FolderMetadata::EncryptedFile::isDirectory() const
{
    return mimetype.isEmpty() || mimetype == QByteArrayLiteral("inode/directory") || mimetype == QByteArrayLiteral("httpd/unix-directory");
}

FolderMetadata::FolderMetadata(AccountPtr account, FolderType folderType)
    : _account(account),
    _isRootEncryptedFolder(folderType == FolderType::Root)
{
    qCInfo(lcCseMetadata()) << "Setting up an Empty Metadata";
    initEmptyMetadata();
}

FolderMetadata::FolderMetadata(AccountPtr account,
                               const QByteArray &metadata,
                               const RootEncryptedFolderInfo &rootEncryptedFolderInfo,
                               const QByteArray &signature,
                               QObject *parent)
    : QObject(parent)
    , _account(account)
    , _initialMetadata(metadata)
    , _isRootEncryptedFolder(rootEncryptedFolderInfo.path == QStringLiteral("/"))
    , _metadataKeyForEncryption(rootEncryptedFolderInfo.keyForEncryption)
    , _metadataKeyForDecryption(rootEncryptedFolderInfo.keyForDecryption)
    , _keyChecksums(rootEncryptedFolderInfo.keyChecksums)
    , _initialSignature(signature)
{
    setupVersionFromExistingMetadata(metadata);

    const auto doc = QJsonDocument::fromJson(metadata);
    qCInfo(lcCseMetadata()) << doc.toJson(QJsonDocument::Compact);
    if (!_isRootEncryptedFolder
        && !rootEncryptedFolderInfo.keysSet()
        && !rootEncryptedFolderInfo.path.isEmpty()) {
        startFetchRootE2eeFolderMetadata(rootEncryptedFolderInfo.path);
    } else {
        initMetadata();
    }
}

void FolderMetadata::initMetadata()
{
    if (_initialMetadata.isEmpty()) {
        qCInfo(lcCseMetadata()) << "Setting up empty metadata";
        initEmptyMetadata();
        return;
    }

    qCInfo(lcCseMetadata()) << "Setting up existing metadata";
    setupExistingMetadata(_initialMetadata);

    if (metadataKeyForDecryption().isEmpty() || metadataKeyForEncryption().isEmpty()) {
        qCWarning(lcCseMetadata()) << "Failed to setup FolderMetadata. Could not parse/create metadataKey!";
    }
    emitSetupComplete();
}

void FolderMetadata::setupExistingMetadata(const QByteArray &metadata)
{
    const auto doc = QJsonDocument::fromJson(metadata);
    qCDebug(lcCseMetadata()) << "Got existing metadata:" << doc.toJson(QJsonDocument::Compact);

    if (_existingMetadataVersion < MetadataVersion::Version1) {
        qCDebug(lcCseMetadata()) << "Could not setup metadata. Incorrect version" << _existingMetadataVersion;
        _account->reportClientStatus(OCC::ClientStatusReportingStatus::E2EeError_GeneralError);
        return;
    }
    if (_existingMetadataVersion < MetadataVersion::Version2_0) {
        setupExistingMetadataLegacy(metadata);
        return;
    }
    
    qCDebug(lcCseMetadata()) << "Setting up latest metadata version" << _existingMetadataVersion;
    const auto metaDataStr = metadataStringFromOCsDocument(doc);
    const auto metaDataDoc = QJsonDocument::fromJson(metaDataStr.toLocal8Bit());

    const auto folderUsers = metaDataDoc[usersKey].toArray();

    const auto isUsersArrayValid = (!_isRootEncryptedFolder && folderUsers.isEmpty()) || (_isRootEncryptedFolder && !folderUsers.isEmpty());
    Q_ASSERT(isUsersArrayValid);

    if (!isUsersArrayValid) {
        qCDebug(lcCseMetadata()) << "Could not decrypt metadata key. Users array is invalid!";
        _account->reportClientStatus(OCC::ClientStatusReportingStatus::E2EeError_GeneralError);
        return;
    }

    if (_isRootEncryptedFolder) {
        QJsonDocument debugHelper;
        debugHelper.setArray(folderUsers);
        qCDebug(lcCseMetadata()) << "users: " << debugHelper.toJson(QJsonDocument::Compact);
    }

    for (auto it = folderUsers.constBegin(); it != folderUsers.constEnd(); ++it) {
        const auto folderUserObject = it->toObject();
        const auto userId = folderUserObject.value(usersUserIdKey).toString();
        UserWithFolderAccess folderUser;
        folderUser.userId = userId;
        /* TODO: does it make sense to store each certificatePem that has been successfuly verified? Is this secure?
        /  Can the attacker use outdated certificate as an attack vector?*/
        folderUser.certificatePem = folderUserObject.value(usersCertificateKey).toString().toUtf8();
        folderUser.encryptedMetadataKey = QByteArray::fromBase64(folderUserObject.value(usersEncryptedMetadataKey).toString().toUtf8());
        _folderUsers[userId] = folderUser;
    }

    if (_isRootEncryptedFolder && !_initialSignature.isEmpty()) {
        const auto metadataForSignature = prepareMetadataForSignature(metaDataDoc);

        QVector<QByteArray> certificatePems;
        certificatePems.reserve(_folderUsers.size());
        for (const auto &folderUser : std::as_const(_folderUsers)) {
            certificatePems.push_back(folderUser.certificatePem);
        }

        if (!_account->e2e()->verifySignatureCryptographicMessageSyntax(QByteArray::fromBase64(_initialSignature), metadataForSignature.toBase64(), certificatePems)) {
            qCDebug(lcCseMetadata()) << "Could not parse encrypred folder metadata. Failed to verify signature!";
            _account->reportClientStatus(OCC::ClientStatusReportingStatus::E2EeError_GeneralError);
            return;
        }
    }

    if (!parseFileDropPart(metaDataDoc)) {
        qCDebug(lcCseMetadata()) << "Could not parse filedrop part";
        return;
    }

    if (_folderUsers.contains(_account->davUser())) {
        const auto currentFolderUser = _folderUsers.value(_account->davUser());
        _metadataKeyForEncryption = decryptDataWithPrivateKey(currentFolderUser.encryptedMetadataKey);
        _metadataKeyForDecryption = _metadataKeyForEncryption;
    }

    if (metadataKeyForDecryption().isEmpty() || metadataKeyForEncryption().isEmpty()) {
        qCDebug(lcCseMetadata()) << "Could not setup metadata key!";
        _account->reportClientStatus(OCC::ClientStatusReportingStatus::E2EeError_GeneralError);
        return;
    }
    
    const auto metadataObj = metaDataDoc.object()[metadataJsonKey].toObject();
    _metadataNonce = QByteArray::fromBase64(metadataObj[nonceKey].toString().toLocal8Bit());
    const auto cipherTextEncrypted = metadataObj[cipherTextKey].toString().toLocal8Bit();

    // for compatibility, the format is "cipheredpart|initializationVector", so we need to extract the "cipheredpart"
    const auto cipherTextPartExtracted = cipherTextEncrypted.split('|').at(0);

    const auto cipherTextDecrypted = EncryptionHelper::decryptThenUnGzipData(metadataKeyForDecryption(), QByteArray::fromBase64(cipherTextPartExtracted), _metadataNonce);
    if (cipherTextDecrypted.isEmpty()) {
        qCDebug(lcCseMetadata()) << "Could not decrypt cipher text!";
        _account->reportClientStatus(OCC::ClientStatusReportingStatus::E2EeError_GeneralError);
        return;
    }

    const auto cipherTextDocument = QJsonDocument::fromJson(cipherTextDecrypted);

    const auto keyCheckSums = cipherTextDocument[keyChecksumsKey].toArray();
    if (!keyCheckSums.isEmpty()) {
        _keyChecksums.clear();
    }
    for (auto it = keyCheckSums.constBegin(); it != keyCheckSums.constEnd(); ++it) {
        const auto keyChecksum = it->toVariant().toString().toUtf8();
        if (!keyChecksum.isEmpty()) {
            //TODO: check that no hash has been removed from the keyChecksums
            // How do we check that?
            _keyChecksums.insert(keyChecksum);
        }
    }

    if (!verifyMetadataKey(metadataKeyForDecryption())) {
        qCDebug(lcCseMetadata()) << "Could not verify metadataKey!";
        _account->reportClientStatus(OCC::ClientStatusReportingStatus::E2EeError_GeneralError);
        return;
    }

    const auto files = cipherTextDocument.object()[filesKey].toObject();
    const auto folders = cipherTextDocument.object()[foldersKey].toObject();

    const auto counterVariantFromJson = cipherTextDocument.object().value(counterKey).toVariant();
    if (counterVariantFromJson.isValid() && counterVariantFromJson.canConvert<quint64>()) {
        // TODO: We need to check counter: new counter must be greater than locally stored counter
        // What does that mean? We store the counter in metadata, should we now store it in local database as we do for all file records in SyncJournal?
        // What if metadata was not updated for a while? The counter will then not be greater than locally stored (in SyncJournal DB?)
        _counter = counterVariantFromJson.value<quint64>();
    }

    for (auto it = files.constBegin(), end = files.constEnd(); it != end; ++it) {
        const auto parsedEncryptedFile = parseEncryptedFileFromJson(it.key(), it.value());
        if (!parsedEncryptedFile.originalFilename.isEmpty()) {
            _files.push_back(parsedEncryptedFile);
        }
    }

    for (auto it = folders.constBegin(); it != folders.constEnd(); ++it) {
        const auto folderName = it.value().toString();
        if (!folderName.isEmpty()) {
            EncryptedFile file;
            file.encryptedFilename = it.key();
            file.originalFilename = folderName;
            _files.push_back(file);
        }
    }
    _isMetadataValid = true;
}

void FolderMetadata::setupExistingMetadataLegacy(const QByteArray &metadata)
{
    const auto doc = QJsonDocument::fromJson(metadata);
    qCDebug(lcCseMetadata()) << "Setting up legacy existing metadata version" << _existingMetadataVersion << doc.toJson(QJsonDocument::Compact);

    const auto metaDataStr = metadataStringFromOCsDocument(doc);
    const auto metaDataDoc = QJsonDocument::fromJson(metaDataStr.toLocal8Bit());
    const auto metadataObj = metaDataDoc.object()[metadataJsonKey].toObject();

    // we will use metadata key from metadata to decrypt legacy metadata, so let's clear the decryption key if any provided by top-level folder
    _metadataKeyForDecryption.clear();

    const auto metadataKeyFromJson = metadataObj[metadataKeyKey].toString().toLocal8Bit();
    if (!metadataKeyFromJson.isEmpty()) {
        // parse version 1.1 and 1.2 (both must have a single "metadataKey"), not "metadataKeys" as 1.0
        const auto decryptedMetadataKeyBase64 = decryptDataWithPrivateKey(QByteArray::fromBase64(metadataKeyFromJson));
        if (!decryptedMetadataKeyBase64.isEmpty()) {
            // fromBase64() multiple times just to stick with the old wrong way
            _metadataKeyForDecryption = QByteArray::fromBase64(QByteArray::fromBase64(decryptedMetadataKeyBase64));
        }
    }

    if (metadataKeyForDecryption().isEmpty() && _existingMetadataVersion < MetadataVersion::Version1_2) {
        // parse version 1.0 (before security-vulnerability fix for metadata keys was released
        qCDebug(lcCseMetadata()) << "Migrating from" << _existingMetadataVersion << "to"
                                 << latestSupportedMetadataVersion();
        const auto metadataKeys = metadataObj["metadataKeys"].toObject();
        if (metadataKeys.isEmpty()) {
            qCDebug(lcCseMetadata()) << "Could not migrate. No metadata keys found!";
            _account->reportClientStatus(OCC::ClientStatusReportingStatus::E2EeError_GeneralError);
            return;
        }

        const auto lastMetadataKeyFromJson = metadataKeys.keys().last().toLocal8Bit();
        if (!lastMetadataKeyFromJson.isEmpty()) {
            const auto lastMetadataKeyValueFromJson = metadataKeys.value(lastMetadataKeyFromJson).toString().toLocal8Bit();
            if (!lastMetadataKeyValueFromJson.isEmpty()) {
                const auto lastMetadataKeyValueFromJsonBase64 = decryptDataWithPrivateKey(QByteArray::fromBase64(lastMetadataKeyValueFromJson));
                if (!lastMetadataKeyValueFromJsonBase64.isEmpty()) {
                    _metadataKeyForDecryption = QByteArray::fromBase64(QByteArray::fromBase64(lastMetadataKeyValueFromJsonBase64));
                }
            }
        }
    }

    if (metadataKeyForDecryption().isEmpty()) {
        qCDebug(lcCseMetadata()) << "Could not setup existing metadata with missing metadataKeys!";
        _account->reportClientStatus(OCC::ClientStatusReportingStatus::E2EeError_GeneralError);
        return;
    }

    if (metadataKeyForEncryption().isEmpty()) {
        _metadataKeyForEncryption = metadataKeyForDecryption();
    }

    const auto sharing = metadataObj["sharing"].toString().toLocal8Bit();
    const auto files = metaDataDoc.object()[filesKey].toObject();
    const auto metadataKey = metaDataDoc.object()[metadataJsonKey].toObject()[metadataKeyKey].toString().toUtf8();
    const auto metadataKeyChecksum = metaDataDoc.object()[metadataJsonKey].toObject()["checksum"].toString().toUtf8();

    setFileDrop(metaDataDoc.object().value("filedrop").toObject());
    // for unit tests
    _fileDropFromServer = _fileDrop;

    for (auto it = files.constBegin(); it != files.constEnd(); ++it) {
        EncryptedFile file;
        file.encryptedFilename = it.key();

        const auto fileObj = it.value().toObject();
        file.authenticationTag = QByteArray::fromBase64(fileObj[authenticationTagKey].toString().toLocal8Bit());
        file.initializationVector = QByteArray::fromBase64(fileObj[initializationVectorKey].toString().toLocal8Bit());

        // Decrypt encrypted part
        const auto encryptedFile = fileObj[encryptedKey].toString().toLocal8Bit();
        const auto decryptedFile = decryptJsonObject(encryptedFile, metadataKeyForDecryption());
        const auto decryptedFileDoc = QJsonDocument::fromJson(decryptedFile);

        const auto decryptedFileObj = decryptedFileDoc.object();

        if (decryptedFileObj["filename"].toString().isEmpty()) {
            qCDebug(lcCseMetadata) << "decrypted metadata" << decryptedFileDoc.toJson(QJsonDocument::Indented);
            qCWarning(lcCseMetadata) << "skipping encrypted file" << file.encryptedFilename << "metadata has an empty file name";
            continue;
        }

        file.originalFilename = decryptedFileObj["filename"].toString();
        file.encryptionKey = QByteArray::fromBase64(decryptedFileObj["key"].toString().toLocal8Bit());
        file.mimetype = decryptedFileObj["mimetype"].toString().toLocal8Bit();

        // In case we wrongly stored "inode/directory" we try to recover from it
        if (file.mimetype == QByteArrayLiteral("inode/directory")) {
            file.mimetype = QByteArrayLiteral("httpd/unix-directory");
        }

        qCDebug(lcCseMetadata) << "encrypted file" << decryptedFileObj["filename"].toString() << decryptedFileObj["key"].toString() << it.key();

        _files.push_back(file);
    }

    if (!checkMetadataKeyChecksum(metadataKey, metadataKeyChecksum) && _existingMetadataVersion >= MetadataVersion::Version1_2) {
        qCInfo(lcCseMetadata) << "checksum comparison failed"
                              << "server value" << metadataKeyChecksum << "client value" << computeMetadataKeyChecksum(metadataKey);
        if (!_account->shouldSkipE2eeMetadataChecksumValidation()) {
            qCDebug(lcCseMetadata) << "Failed to validate checksum for legacy metadata!";
            _account->reportClientStatus(OCC::ClientStatusReportingStatus::E2EeError_GeneralError);
            return;
        }
        qCDebug(lcCseMetadata) << "shouldSkipE2eeMetadataChecksumValidation is set. Allowing invalid checksum until next sync.";
    }
    _isMetadataValid = true;
}

void FolderMetadata::setupVersionFromExistingMetadata(const QByteArray &metadata)
{
    const auto doc = QJsonDocument::fromJson(metadata);
    const auto metaDataStr = metadataStringFromOCsDocument(doc);
    const auto metaDataDoc = QJsonDocument::fromJson(metaDataStr.toLocal8Bit());
    const auto metadataObj = metaDataDoc.object()[metadataJsonKey].toObject();

    QString versionStringFromMetadata;

    if (metadataObj.contains(versionKey)) {
        const auto metadataVersionValue = metadataObj.value(versionKey);
        if (metadataVersionValue.type() == QJsonValue::Type::String) {
            versionStringFromMetadata = metadataObj[versionKey].toString();
        } else if (metadataVersionValue.type() == QJsonValue::Type::Double) {
            versionStringFromMetadata = QString::number(metadataVersionValue.toDouble(), 'f', 1);
        }
    }
    else if (metaDataDoc.object().contains(versionKey)) {
        const auto metadataVersionValue = metaDataDoc.object()[versionKey].toVariant();
        if (metadataVersionValue.type() == QVariant::Type::String) {
            versionStringFromMetadata = metadataVersionValue.toString();
        } else if (metadataVersionValue.type() == QVariant::Type::Double) {
            versionStringFromMetadata = QString::number(metadataVersionValue.toDouble(), 'f', 1);
        } else if (metadataVersionValue.type() == QVariant::Type::Int) {
            versionStringFromMetadata = QString::number(metadataVersionValue.toInt()) + QStringLiteral(".0");
        }
    }

    if (versionStringFromMetadata == QStringLiteral("1.2")) {
        _existingMetadataVersion = MetadataVersion::Version1_2;
    } else if (versionStringFromMetadata == QStringLiteral("2.0") || versionStringFromMetadata == QStringLiteral("2")) {
        _existingMetadataVersion = MetadataVersion::Version2_0;
    } else if (versionStringFromMetadata == QStringLiteral("1.0")
        || versionStringFromMetadata == QStringLiteral("1.1")) {
        // We used to have an intermediate 1.1 after applying a security-vulnerability fix for metadata keys.
        // It should be treated as MetadataVersion::Version1, as we don't want to change logic related to 1.2, since 1.1 is an edge case.
        _existingMetadataVersion = MetadataVersion::Version1;
    }
}

void FolderMetadata::emitSetupComplete()
{
    QTimer::singleShot(0, this, [this]() {
        emit setupComplete();
    });
}

// RSA/ECB/OAEPWithSHA-256AndMGF1Padding using private / public key.
QByteArray FolderMetadata::encryptDataWithPublicKey(const QByteArray &data, const QSslKey &key) const
{
    Bio publicKeyBio;
    const auto publicKeyPem = key.toPem();
    BIO_write(publicKeyBio, publicKeyPem.constData(), publicKeyPem.size());
    const auto publicKey = PKey::readPublicKey(publicKeyBio);

    // The metadata key is binary so base64 encode it first
    return EncryptionHelper::encryptStringAsymmetric(publicKey, data);
}

QByteArray FolderMetadata::decryptDataWithPrivateKey(const QByteArray &data) const
{
    Bio privateKeyBio;
    BIO_write(privateKeyBio, _account->e2e()->_privateKey.constData(), _account->e2e()->_privateKey.size());
    const auto privateKey = PKey::readPrivateKey(privateKeyBio);

    const auto decryptResult = EncryptionHelper::decryptStringAsymmetric(privateKey, data);
    if (decryptResult.isEmpty()) {
        qCDebug(lcCseMetadata()) << "ERROR. Could not decrypt the metadata key";
        _account->reportClientStatus(OCC::ClientStatusReportingStatus::E2EeError_GeneralError);
    }
    return decryptResult;
}

// AES/GCM/NoPadding (128 bit key size)
QByteArray FolderMetadata::encryptJsonObject(const QByteArray& obj, const QByteArray pass) const
{
    return EncryptionHelper::encryptStringSymmetric(pass, obj);
}

QByteArray FolderMetadata::decryptJsonObject(const QByteArray& encryptedMetadata, const QByteArray& pass) const
{
    return EncryptionHelper::decryptStringSymmetric(pass, encryptedMetadata);
}

bool FolderMetadata::checkMetadataKeyChecksum(const QByteArray &metadataKey, const QByteArray &metadataKeyChecksum) const
{
    const auto referenceMetadataKeyValue = computeMetadataKeyChecksum(metadataKey);

    return referenceMetadataKeyValue == metadataKeyChecksum;
}

QByteArray FolderMetadata::computeMetadataKeyChecksum(const QByteArray &metadataKey) const
{
    auto hashAlgorithm = QCryptographicHash{QCryptographicHash::Sha256};

    hashAlgorithm.addData(_account->e2e()->_mnemonic.remove(' ').toUtf8());
    auto sortedFiles = _files;
    std::sort(sortedFiles.begin(), sortedFiles.end(), [](const auto &first, const auto &second) {
        return first.encryptedFilename < second.encryptedFilename;
    });
    for (const auto &singleFile : sortedFiles) {
        hashAlgorithm.addData(singleFile.encryptedFilename.toUtf8());
    }
    hashAlgorithm.addData(metadataKey);

    return hashAlgorithm.result().toHex();
}

bool FolderMetadata::isValid() const
{
    return _isMetadataValid;
}

FolderMetadata::EncryptedFile FolderMetadata::parseEncryptedFileFromJson(const QString &encryptedFilename, const QJsonValue &fileJSON) const
{
    const auto fileObj = fileJSON.toObject();
    if (fileObj["filename"].toString().isEmpty()) {
        qCWarning(lcCseMetadata()) << "skipping encrypted file" << encryptedFilename << "metadata has an empty file name";
        return {};
    }
    
    EncryptedFile file;
    file.encryptedFilename = encryptedFilename;
    file.authenticationTag = QByteArray::fromBase64(fileObj[authenticationTagKey].toString().toLocal8Bit());
    auto nonce = QByteArray::fromBase64(fileObj[initializationVectorKey].toString().toLocal8Bit());
    if (nonce.isEmpty()) {
        nonce = QByteArray::fromBase64(fileObj[nonceKey].toString().toLocal8Bit());
    }
    file.initializationVector = nonce;
    file.originalFilename = fileObj["filename"].toString();
    file.encryptionKey = QByteArray::fromBase64(fileObj["key"].toString().toLocal8Bit());
    file.mimetype = fileObj["mimetype"].toString().toLocal8Bit();

    // In case we wrongly stored "inode/directory" we try to recover from it
    if (file.mimetype == QByteArrayLiteral("inode/directory")) {
        file.mimetype = QByteArrayLiteral("httpd/unix-directory");
    }

    return file;
}

QJsonObject FolderMetadata::convertFileToJsonObject(const EncryptedFile *encryptedFile) const
{
    QJsonObject file;
    file.insert("key", QString(encryptedFile->encryptionKey.toBase64()));
    file.insert("filename", encryptedFile->originalFilename);
    file.insert("mimetype", QString(encryptedFile->mimetype));
    const auto nonceFinalKey = latestSupportedMetadataVersion() < MetadataVersion::Version2_0
        ? initializationVectorKey
        : nonceKey;
    file.insert(nonceFinalKey, QString(encryptedFile->initializationVector.toBase64()));
    file.insert(authenticationTagKey, QString(encryptedFile->authenticationTag.toBase64()));

    return file;
}

const QByteArray FolderMetadata::metadataKeyForEncryption() const
{
    return _metadataKeyForEncryption;
}

const QSet<QByteArray>& FolderMetadata::keyChecksums() const
{
    return _keyChecksums;
}

void FolderMetadata::initEmptyMetadata()
{
    if (_account->capabilities().clientSideEncryptionVersion() < 2.0) {
        return initEmptyMetadataLegacy();
    }
    qCDebug(lcCseMetadata()) << "Setting up empty metadata v2";
    if (_isRootEncryptedFolder) {
        if (!addUser(_account->davUser(), _account->e2e()->_certificate)) {
            qCDebug(lcCseMetadata) << "Empty metadata setup failed. Could not add first user.";
            _account->reportClientStatus(OCC::ClientStatusReportingStatus::E2EeError_GeneralError);
            return;
        }
        _metadataKeyForDecryption = _metadataKeyForEncryption;
    }
    _isMetadataValid = true;

    emitSetupComplete();
}

void FolderMetadata::initEmptyMetadataLegacy()
{
    qCDebug(lcCseMetadata) << "Settint up legacy empty metadata";
    _metadataKeyForEncryption = EncryptionHelper::generateRandom(metadataKeySize);
    _metadataKeyForDecryption = _metadataKeyForEncryption;
    QString publicKey = _account->e2e()->_publicKey.toPem().toBase64();
    QString displayName = _account->displayName();

    _isMetadataValid = true;

    emitSetupComplete();
}

QByteArray FolderMetadata::encryptedMetadata()
{
    Q_ASSERT(_isMetadataValid);
    if (!_isMetadataValid) {
        qCCritical(lcCseMetadata()) << "Could not encrypt non-initialized metadata!";
        return {};
    }

    if (latestSupportedMetadataVersion() < MetadataVersion::Version2_0) {
        return encryptedMetadataLegacy();
    }

    qCDebug(lcCseMetadata()) << "Encrypting metadata for latest version"
                             << latestSupportedMetadataVersion();
    if (_isRootEncryptedFolder && _folderUsers.isEmpty() && _existingMetadataVersion < MetadataVersion::Version2_0) {
        // migrated from legacy version, create metadata key and setup folderUsrs array
        createNewMetadataKeyForEncryption();
    }

    if (metadataKeyForEncryption().isEmpty()) {
        qCDebug(lcCseMetadata()) << "Encrypting metadata failed! Empty metadata key!";
        return {};
    }

    QJsonObject files, folders;
    for (auto it = _files.constBegin(), end = _files.constEnd(); it != end; ++it) {
        const auto file = convertFileToJsonObject(it);
        if (file.isEmpty()) {
            qCDebug(lcCseMetadata) << "Metadata generation failed for file" << it->encryptedFilename;
            return {};
        }
        const auto isDirectory =
            it->mimetype.isEmpty() || it->mimetype == QByteArrayLiteral("inode/directory") || it->mimetype == QByteArrayLiteral("httpd/unix-directory");
        if (isDirectory) {
            folders.insert(it->encryptedFilename, it->originalFilename);
        } else {
            files.insert(it->encryptedFilename, file);
        }
    }

    QJsonArray keyChecksums;
    if (_isRootEncryptedFolder) {
        for (auto it = _keyChecksums.constBegin(), end = _keyChecksums.constEnd(); it != end; ++it) {
            keyChecksums.push_back(QJsonValue::fromVariant(*it));
        }
    }

    QJsonObject cipherText = {{counterKey, QJsonValue::fromVariant(newCounter())}, {filesKey, files}, {foldersKey, folders}};

    const auto isChecksumsArrayValid = (!_isRootEncryptedFolder && keyChecksums.isEmpty()) || (_isRootEncryptedFolder && !keyChecksums.isEmpty());
    Q_ASSERT(isChecksumsArrayValid);
    if (!isChecksumsArrayValid) {
        qCDebug(lcCseMetadata) << "Empty keyChecksums while shouldn't be empty!";
        return {};
    }
    if (!keyChecksums.isEmpty()) {
        cipherText.insert(keyChecksumsKey, keyChecksums);
    }

    const QJsonDocument cipherTextDoc(cipherText);

    QByteArray authenticationTag;
    const auto initializationVector = EncryptionHelper::generateRandom(metadataKeySize);
    const auto initializationVectorBase64 = initializationVector.toBase64();
    const auto gzippedThenEncryptData = EncryptionHelper::gzipThenEncryptData(metadataKeyForEncryption(), cipherTextDoc.toJson(QJsonDocument::Compact), initializationVector, authenticationTag).toBase64();
    // backwards compatible with old versions ("ciphertext|initializationVector")
    const auto encryptedCipherText = QByteArray(gzippedThenEncryptData + QByteArrayLiteral("|") + initializationVectorBase64);
    const QJsonObject metadata{{cipherTextKey, QJsonValue::fromVariant(encryptedCipherText)},
                               {nonceKey, QJsonValue::fromVariant(initializationVectorBase64)},
                               {authenticationTagKey, QJsonValue::fromVariant(authenticationTag.toBase64())}};

    QJsonObject metaObject = {{metadataJsonKey, metadata}, {versionKey, QString::number(_account->capabilities().clientSideEncryptionVersion(), 'f', 1)}};

    QJsonArray folderUsers;
    if (_isRootEncryptedFolder) {
        for (auto it = _folderUsers.constBegin(), end = _folderUsers.constEnd(); it != end; ++it) {
            const auto folderUser = it.value();

            const QJsonObject folderUserJson{{usersUserIdKey, folderUser.userId},
                                             {usersCertificateKey, QJsonValue::fromVariant(folderUser.certificatePem)},
                                             {usersEncryptedMetadataKey, QJsonValue::fromVariant(folderUser.encryptedMetadataKey.toBase64())}};
            folderUsers.push_back(folderUserJson);
        }
    }
    const auto isFolderUsersArrayValid = (!_isRootEncryptedFolder && folderUsers.isEmpty()) || (_isRootEncryptedFolder && !folderUsers.isEmpty());
    Q_ASSERT(isFolderUsersArrayValid);
    if (!isFolderUsersArrayValid) {
        qCCritical(lcCseMetadata) << "Empty folderUsers while shouldn't be empty!";
        return {};
    }

    if (!folderUsers.isEmpty()) {
        metaObject.insert(usersKey, folderUsers);
    }

    if (!_fileDrop.isEmpty()) {
        // if we did not consume _fileDrop, we must keep it where it was, on the server
        metaObject.insert(filedropKey, _fileDrop);
    }

    QJsonDocument internalMetadata;
    internalMetadata.setObject(metaObject);

    const auto jsonString = internalMetadata.toJson();

    const auto metadataForSignature = prepareMetadataForSignature(internalMetadata);
    _metadataSignature = _account->e2e()->generateSignatureCryptographicMessageSyntax(metadataForSignature.toBase64()).toBase64();

    _encryptedMetadataVersion = latestSupportedMetadataVersion();

    return jsonString;
}

QByteArray FolderMetadata::encryptedMetadataLegacy()
{
    qCDebug(lcCseMetadata) << "Generating metadata";

    if (_metadataKeyForEncryption.isEmpty()) {
        qCDebug(lcCseMetadata) << "Metadata generation failed! Empty metadata key!";
        _account->reportClientStatus(OCC::ClientStatusReportingStatus::E2EeError_GeneralError);
        return {};
    }
    const auto version = _account->capabilities().clientSideEncryptionVersion();
    // multiple toBase64() just to keep with the old (wrong way)
    const auto encryptedMetadataKey = encryptDataWithPublicKey(metadataKeyForEncryption().toBase64().toBase64(), _account->e2e()->_publicKey).toBase64();
    const QJsonObject metadata{
        {versionKey, QString::number(version, 'f', 1)},
        {metadataKeyKey, QJsonValue::fromVariant(encryptedMetadataKey)},
        {"checksum", QJsonValue::fromVariant(computeMetadataKeyChecksum(encryptedMetadataKey))},
    };

    QJsonObject files;
    for (auto it = _files.constBegin(), end = _files.constEnd(); it != end; ++it) {
        QJsonObject encrypted;
        encrypted.insert("key", QString(it->encryptionKey.toBase64()));
        encrypted.insert("filename", it->originalFilename);
        encrypted.insert("mimetype", QString(it->mimetype));
        QJsonDocument encryptedDoc;
        encryptedDoc.setObject(encrypted);

        QString encryptedEncrypted = encryptJsonObject(encryptedDoc.toJson(QJsonDocument::Compact), metadataKeyForEncryption());
        if (encryptedEncrypted.isEmpty()) {
            qCDebug(lcCseMetadata) << "Metadata generation failed!";
            _account->reportClientStatus(OCC::ClientStatusReportingStatus::E2EeError_GeneralError);
        }
        QJsonObject file;
        file.insert(encryptedKey, encryptedEncrypted);
        file.insert(initializationVectorKey, QString(it->initializationVector.toBase64()));
        file.insert(authenticationTagKey, QString(it->authenticationTag.toBase64()));

        files.insert(it->encryptedFilename, file);
    }

    QJsonObject filedrop;
    for (auto fileDropIt = _fileDrop.constBegin(), end = _fileDrop.constEnd(); fileDropIt != end; ++fileDropIt) {
        filedrop.insert(fileDropIt.key(), fileDropIt.value());
    }

    auto metaObject = QJsonObject{
        {metadataJsonKey, metadata},
    };

    if (files.count()) {
        metaObject.insert(filesKey, files);
    }

    if (filedrop.count()) {
        metaObject.insert(filedropKey, filedrop);
    }

    _encryptedMetadataVersion = fromItemEncryptionStatusToMedataVersion(EncryptionStatusEnums::fromEndToEndEncryptionApiVersion(version));

    QJsonDocument internalMetadata;
    internalMetadata.setObject(metaObject);
    return internalMetadata.toJson();
}

EncryptionStatusEnums::ItemEncryptionStatus FolderMetadata::existingMetadataEncryptionStatus() const
{
    return FolderMetadata::fromMedataVersionToItemEncryptionStatus(_existingMetadataVersion);
}

EncryptionStatusEnums::ItemEncryptionStatus FolderMetadata::encryptedMetadataEncryptionStatus() const
{
    return FolderMetadata::fromMedataVersionToItemEncryptionStatus(_encryptedMetadataVersion);
}

bool FolderMetadata::isVersion2AndUp() const
{
    return _existingMetadataVersion >= MetadataVersion::Version2_0;
}

FolderMetadata::MetadataVersion FolderMetadata::latestSupportedMetadataVersion() const
{
    const auto itemEncryptionStatusFromApiVersion = EncryptionStatusEnums::fromEndToEndEncryptionApiVersion(_account->capabilities().clientSideEncryptionVersion());
    return fromItemEncryptionStatusToMedataVersion(itemEncryptionStatusFromApiVersion);
}

bool FolderMetadata::parseFileDropPart(const QJsonDocument &doc)
{
    const auto fileDropObject = doc.object().value(filedropKey).toObject();
    const auto fileDropMap = fileDropObject.toVariantMap();

    for (auto it = std::cbegin(fileDropMap); it != std::cend(fileDropMap); ++it) {
        const auto fileDropEntryParsed = it.value().toMap();
        FileDropEntry fileDropEntry{it.key(),
                                    fileDropEntryParsed.value(cipherTextKey).toByteArray(),
                                    QByteArray::fromBase64(fileDropEntryParsed.value(nonceKey).toByteArray()),
                                    QByteArray::fromBase64(fileDropEntryParsed.value(authenticationTagKey).toByteArray()),
                                    {}};
        const auto usersRaw = fileDropEntryParsed.value(usersKey).toList();
        for (const auto &userRaw : usersRaw) {
            const auto userParsed = userRaw.toMap();
            const auto userParsedId = userParsed.value(usersUserIdKey).toByteArray();
            if (userParsedId == _account->davUser()) {
                const auto fileDropEntryUser = UserWithFileDropEntryAccess{
                    userParsedId,
                    decryptDataWithPrivateKey(QByteArray::fromBase64(userParsed.value(usersEncryptedFiledropKey).toByteArray()))};
                if (!fileDropEntryUser.isValid()) {
                    qCDebug(lcCseMetadata()) << "Could not parse filedrop data. encryptedFiledropKey decryption failed";
                    _account->reportClientStatus(OCC::ClientStatusReportingStatus::E2EeError_GeneralError);
                    return false;
                }
                fileDropEntry.currentUser = fileDropEntryUser;
                break;
            }
        }
        if (!fileDropEntry.isValid()) {
            qCDebug(lcCseMetadata()) << "Could not parse filedrop data. fileDropEntry is invalid for userId" << fileDropEntry.currentUser.userId;
            _account->reportClientStatus(OCC::ClientStatusReportingStatus::E2EeError_GeneralError);
            return false;
        }
        if (fileDropEntry.currentUser.isValid()) {
            _fileDropEntries.push_back(fileDropEntry);
        }
    }
    return true;
}

void FolderMetadata::setFileDrop(const QJsonObject &fileDrop)
{
    _fileDrop = fileDrop;
}

QByteArray FolderMetadata::metadataSignature() const
{
    return _metadataSignature;
}

QByteArray FolderMetadata::initialMetadata() const
{
    return _initialMetadata;
}

quint64 FolderMetadata::newCounter() const
{
    return _counter + 1;
}

EncryptionStatusEnums::ItemEncryptionStatus FolderMetadata::fromMedataVersionToItemEncryptionStatus(const MetadataVersion metadataVersion)
{
    switch (metadataVersion) {
    case FolderMetadata::MetadataVersion::Version2_0:
        return SyncFileItem::EncryptionStatus::EncryptedMigratedV2_0;
    case FolderMetadata::MetadataVersion::Version1_2:
        return SyncFileItem::EncryptionStatus::EncryptedMigratedV1_2;
    case FolderMetadata::MetadataVersion::Version1:
        return SyncFileItem::EncryptionStatus::Encrypted;
    case FolderMetadata::MetadataVersion::VersionUndefined:
        return SyncFileItem::EncryptionStatus::NotEncrypted;
    }
    return SyncFileItem::EncryptionStatus::NotEncrypted;
}

FolderMetadata::MetadataVersion FolderMetadata::fromItemEncryptionStatusToMedataVersion(const EncryptionStatusEnums::ItemEncryptionStatus encryptionStatus)
{
    switch (encryptionStatus) {
    case EncryptionStatusEnums::ItemEncryptionStatus::Encrypted:
        return MetadataVersion::Version1;
    case EncryptionStatusEnums::ItemEncryptionStatus::EncryptedMigratedV1_2:
        return MetadataVersion::Version1_2;
    case EncryptionStatusEnums::ItemEncryptionStatus::EncryptedMigratedV2_0:
        return MetadataVersion::Version2_0;
    case EncryptionStatusEnums::ItemEncryptionStatus::NotEncrypted:
        return MetadataVersion::VersionUndefined;
    }
    return MetadataVersion::VersionUndefined;
}

QByteArray FolderMetadata::prepareMetadataForSignature(const QJsonDocument &fullMetadata)
{
    auto metdataModified = fullMetadata;

    auto modifiedObject = metdataModified.object();
    modifiedObject.remove(filedropKey);

    if (modifiedObject.contains(usersKey)) {
        const auto folderUsers = modifiedObject[usersKey].toArray();

        QJsonArray modofiedFolderUsers;

        for (auto it = folderUsers.constBegin(); it != folderUsers.constEnd(); ++it) {
            auto folderUserObject = it->toObject();
            folderUserObject.remove(usersEncryptedFiledropKey);
            modofiedFolderUsers.push_back(folderUserObject);
        }
        modifiedObject.insert(usersKey, modofiedFolderUsers);
    }

    metdataModified.setObject(modifiedObject);
    return metdataModified.toJson(QJsonDocument::Compact);
}

void FolderMetadata::addEncryptedFile(const EncryptedFile &f) {
    Q_ASSERT(_isMetadataValid);
    if (!_isMetadataValid) {
        qCCritical(lcCseMetadata()) << "Could not add encrypted file to non-initialized metadata!";
        return;
    }

    for (int i = 0; i < _files.size(); ++i) {
        if (_files.at(i).originalFilename == f.originalFilename) {
            _files.removeAt(i);
            break;
        }
    }

    _files.append(f);
}

const QByteArray FolderMetadata::metadataKeyForDecryption() const
{
    return _metadataKeyForDecryption;
}

void FolderMetadata::removeEncryptedFile(const EncryptedFile &f)
{
    for (int i = 0; i < _files.size(); ++i) {
        if (_files.at(i).originalFilename == f.originalFilename) {
            _files.removeAt(i);
            break;
        }
    }
}

void FolderMetadata::removeAllEncryptedFiles()
{
    _files.clear();
}

QVector<FolderMetadata::EncryptedFile> FolderMetadata::files() const
{
    return _files;
}

bool FolderMetadata::isFileDropPresent() const
{
    return !_fileDropEntries.isEmpty();
}

bool FolderMetadata::isRootEncryptedFolder() const
{
    return _isRootEncryptedFolder;
}

bool FolderMetadata::encryptedMetadataNeedUpdate() const
{
    // TODO: For now we do not migrated to V2 if a folder has subfolders, remove the following code and only leave "return latestSupportedMetadataVersion() > _existingMetadataVersion;"
    if (latestSupportedMetadataVersion() <= _existingMetadataVersion) {
        return false;
    }

    const auto foundNestedFoldersOrIsNestedFolder = !_isRootEncryptedFolder
        || std::find_if(std::cbegin(_files), std::cend(_files), [](const auto &file) { return file.isDirectory(); }) != std::cend(_files);

    return !foundNestedFoldersOrIsNestedFolder;
}

bool FolderMetadata::moveFromFileDropToFiles()
{
    if (_fileDropEntries.isEmpty()) {
        return false;
    }

    for (const auto &fileDropEntry : std::as_const(_fileDropEntries)) {
        const auto cipherTextDecrypted = EncryptionHelper::decryptThenUnGzipData(
            fileDropEntry.currentUser.decryptedFiledropKey,
            QByteArray::fromBase64(fileDropEntry.cipherText),
            fileDropEntry.nonce);

        if (cipherTextDecrypted.isEmpty()) {
            qCDebug(lcCseMetadata()) << "Could not decrypt filedrop metadata.";
            _account->reportClientStatus(OCC::ClientStatusReportingStatus::E2EeError_GeneralError);
            return false;
        }

        const auto cipherTextDocument = QJsonDocument::fromJson(cipherTextDecrypted);
        const auto parsedEncryptedFile = parseEncryptedFileFromJson(fileDropEntry.encryptedFilename, cipherTextDocument.object());
        if (parsedEncryptedFile.originalFilename.isEmpty()) {
            qCDebug(lcCseMetadata()) << "Could parse filedrop metadata. Encrypted file" << parsedEncryptedFile.encryptedFilename << "metadata is invalid";
            _account->reportClientStatus(OCC::ClientStatusReportingStatus::E2EeError_GeneralError);
            return false;
        }
        if (parsedEncryptedFile.mimetype.isEmpty()) {
            qCDebug(lcCseMetadata()) << "Could parse filedrop metadata. mimetype is empty for file" << parsedEncryptedFile.originalFilename;
            _account->reportClientStatus(OCC::ClientStatusReportingStatus::E2EeError_GeneralError);
            return false;
        }
        addEncryptedFile(parsedEncryptedFile);
    }

    _fileDropEntries.clear();
    setFileDrop({});

    return true;
}

void FolderMetadata::startFetchRootE2eeFolderMetadata(const QString &path)
{
    _encryptedFolderMetadataHandler.reset(new EncryptedFolderMetadataHandler(_account, path, nullptr, "/"));

    connect(_encryptedFolderMetadataHandler.data(),
            &EncryptedFolderMetadataHandler::fetchFinished,
            this,
            &FolderMetadata::slotRootE2eeFolderMetadataReceived);
    _encryptedFolderMetadataHandler->fetchMetadata(RootEncryptedFolderInfo::makeDefault(), EncryptedFolderMetadataHandler::FetchMode::AllowEmptyMetadata);
}

void FolderMetadata::slotRootE2eeFolderMetadataReceived(int statusCode, const QString &message)
{
    Q_UNUSED(message);
    if (statusCode != 200) {
        qCDebug(lcCseMetadata()) << "Could not fetch root folder metadata" << statusCode << message;
        _account->reportClientStatus(OCC::ClientStatusReportingStatus::E2EeError_GeneralError);
    }
    const auto rootE2eeFolderMetadata = _encryptedFolderMetadataHandler->folderMetadata();
    if (statusCode != 200 || !rootE2eeFolderMetadata->isValid() || !rootE2eeFolderMetadata->isVersion2AndUp()) {
        initMetadata();
        return;
    }
    
    _metadataKeyForEncryption = rootE2eeFolderMetadata->metadataKeyForEncryption();
    
    if (!isVersion2AndUp()) {
        initMetadata();
        return;
    }

    _metadataKeyForDecryption = rootE2eeFolderMetadata->metadataKeyForDecryption();
    _metadataKeyForEncryption = rootE2eeFolderMetadata->metadataKeyForEncryption();
    _keyChecksums = rootE2eeFolderMetadata->keyChecksums();
    initMetadata();
}

bool FolderMetadata::addUser(const QString &userId, const QSslCertificate &certificate)
{
    Q_ASSERT(_isRootEncryptedFolder);
    if (!_isRootEncryptedFolder) {
        qCWarning(lcCseMetadata()) << "Could not add a folder user to a non top level folder.";
        return false;
    }

    const auto certificatePublicKey = certificate.publicKey();
    if (userId.isEmpty() || certificate.isNull() || certificatePublicKey.isNull()) {
        qCWarning(lcCseMetadata()) << "Could not add a folder user. Invalid userId or certificate.";
        return false;
    }

    createNewMetadataKeyForEncryption();
    UserWithFolderAccess newFolderUser;
    newFolderUser.userId = userId;
    newFolderUser.certificatePem = certificate.toPem();
    newFolderUser.encryptedMetadataKey = encryptDataWithPublicKey(metadataKeyForEncryption(), certificatePublicKey);
    _folderUsers[userId] = newFolderUser;
    updateUsersEncryptedMetadataKey();

    return true;
}

bool FolderMetadata::removeUser(const QString &userId)
{
    Q_ASSERT(_isRootEncryptedFolder);
    if (!_isRootEncryptedFolder) {
        qCWarning(lcCseMetadata()) << "Could not add remove folder user from a non top level folder.";
        return false;
    }
    Q_ASSERT(!userId.isEmpty());
    if (userId.isEmpty()) {
        qCDebug(lcCseMetadata()) << "Could not remove a folder user. Invalid userId.";
        return false;
    }

    createNewMetadataKeyForEncryption();
    _folderUsers.remove(userId);
    updateUsersEncryptedMetadataKey();

    return true;
}

void FolderMetadata::updateUsersEncryptedMetadataKey()
{
    Q_ASSERT(_isRootEncryptedFolder);
    if (!_isRootEncryptedFolder) {
        qCWarning(lcCseMetadata()) << "Could not update folder users in a non top level folder.";
        return;
    }
    Q_ASSERT(!metadataKeyForEncryption().isEmpty());
    if (metadataKeyForEncryption().isEmpty()) {
        qCWarning(lcCseMetadata()) << "Could not update folder users with empty metadataKey!";
        return;
    }
    for (auto it = _folderUsers.constBegin(); it != _folderUsers.constEnd(); ++it) {
        auto folderUser = it.value();

        const QSslCertificate certificate(folderUser.certificatePem);
        const auto certificatePublicKey = certificate.publicKey();
        if (certificate.isNull() || certificatePublicKey.isNull()) {
            qCWarning(lcCseMetadata()) << "Could not update folder users with null certificatePublicKey!";
            continue;
        }

        const auto encryptedMetadataKey = encryptDataWithPublicKey(metadataKeyForEncryption(), certificatePublicKey);
        if (encryptedMetadataKey.isEmpty()) {
            qCWarning(lcCseMetadata()) << "Could not update folder users with empty encryptedMetadataKey!";
            continue;
        }

        folderUser.encryptedMetadataKey = encryptedMetadataKey;

        _folderUsers[it.key()] = folderUser;
    }
}

void FolderMetadata::createNewMetadataKeyForEncryption()
{
    if (!_isRootEncryptedFolder) {
        return;
    }
    _metadataKeyForEncryption = EncryptionHelper::generateRandom(metadataKeySize);
    if (!metadataKeyForEncryption().isEmpty()) {
        _keyChecksums.insert(calcSha256(metadataKeyForEncryption()));
    }
}

bool FolderMetadata::verifyMetadataKey(const QByteArray &metadataKey) const
{
    if (_existingMetadataVersion < MetadataVersion::Version2_0) {
        return true;
    }
    if (metadataKey.isEmpty() || metadataKey.size() < metadataKeySize ) {
        return false;
    }
    const QByteArray metadataKeyLimitedLength(metadataKey.data(), metadataKeySize );
    // _keyChecksums should not be empty, fix this by taking a proper _keyChecksums from the topLevelFolder
    return _keyChecksums.contains(calcSha256(metadataKeyLimitedLength)) || _keyChecksums.isEmpty();
}
}
