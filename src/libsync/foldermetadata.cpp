#include "account.h"
#include "foldermetadata.h"
#include "clientsideencryption.h"
#include "clientsideencryptionjobs.h"
#include <common/checksums.h>
#include <KCompressionDevice>
#include <QJsonArray>
#include <QJsonDocument>

namespace OCC
{
Q_LOGGING_CATEGORY(lcCseMetadata, "nextcloud.metadata", QtInfoMsg)

namespace
{
constexpr auto authenticationTagKey = "authenticationTag";
constexpr auto cipherTextKey = "ciphertext";
constexpr auto filesKey = "files";
constexpr auto filedropKey = "filedrop";
constexpr auto foldersKey = "folders";
constexpr auto initializationVectorKey = "initializationVector";
constexpr auto keyChecksumsKey = "keyChecksums";
constexpr auto metadataJsonKey = "metadata";
constexpr auto metadataKeyKey = "metadataKey";
constexpr auto metadataKeysKey = "metadataKeys";
constexpr auto nonceKey = "nonce";
constexpr auto sharingKey = "sharing";
constexpr auto usersKey = "users";
constexpr auto usersUserIdKey = "userId";
constexpr auto usersCertificateKey = "certificate";
constexpr auto usersEncryptedMetadataKey = "encryptedMetadataKey";
constexpr auto usersEncryptedFiledropKey = "encryptedFiledropKey";
constexpr auto versionKey = "version";

QString metadataStringFromOCsDocument(const QJsonDocument &ocsDoc)
{
    return ocsDoc.object()["ocs"].toObject()["data"].toObject()["meta-data"].toString();
}
}

FolderMetadata::FolderMetadata(AccountPtr account,
                               const QByteArray &metadata,
                               const QString &topLevelFolderPath,
                               QObject *parent)
    : QObject(parent)
    , _account(account)
    , _initialMetadata(metadata)
{
    _topLevelFolderPath = topLevelFolderPath;

    QJsonDocument doc = QJsonDocument::fromJson(metadata);
    qCInfo(lcCseMetadata()) << doc.toJson(QJsonDocument::Compact);
    const auto metaDataStr = metadataStringFromOCsDocument(doc);

    QJsonDocument metaDataDoc = QJsonDocument::fromJson(metaDataStr.toLocal8Bit());
    QJsonObject metadataObj = metaDataDoc.object()[metadataJsonKey].toObject();
    QJsonObject metadataKeys = metadataObj[metadataKeysKey].toObject();
    const auto folderUsers = metaDataDoc[usersKey].toArray();
    if (metadataObj.contains(versionKey)) {
        _versionFromMetadata = metadataObj[versionKey].toInt();
    }
    if (metaDataDoc.object().contains(versionKey)) {
        _versionFromMetadata = metaDataDoc.object()[versionKey].toInt();
    }

    if (!isTopLevelFolder() && (_versionFromMetadata == -1 || _versionFromMetadata >= 2)) {
        startFetchTopLevelFolderMetadata();
    } else {
        setupMetadata();
    }
}

void FolderMetadata::setupMetadata()
{
    if (_initialMetadata.isEmpty()) {
        qCInfo(lcCseMetadata()) << "Setupping Empty Metadata";
        if (_topLevelFolderMetadata && _topLevelFolderMetadata->versionFromMetadata() == 1) {
            setupEmptyMetadataV1();
        } else {
            setupEmptyMetadataV2();
        }
    } else {
        qCInfo(lcCseMetadata()) << "Setting up existing metadata";
        setupExistingMetadata(_initialMetadata);
    }

    if (_metadataKey.isEmpty()) {
        if (_topLevelFolderMetadata) {
            _metadataKey = _topLevelFolderMetadata->metadataKey();
        }
    }

    if (_metadataKey.isEmpty()) {
        qCWarning(lcCseMetadata()) << "Failed to setup FolderMetadata. Could not parse/create _metadataKey!";
    }

    emitSetupComplete();
}

void FolderMetadata::setupExistingMetadata(const QByteArray &metadata)
{
    const auto doc = QJsonDocument::fromJson(metadata);
    qCInfo(lcCseMetadata()) << doc.toJson(QJsonDocument::Compact);

    const auto metaDataStr = metadataStringFromOCsDocument(doc);

    const auto metaDataDoc = QJsonDocument::fromJson(metaDataStr.toLocal8Bit());
    const auto metadataObj = metaDataDoc.object()[metadataJsonKey].toObject();
    _versionFromMetadata = metadataObj.contains(versionKey) ? metadataObj[versionKey].toInt() : metaDataDoc.object()[versionKey].toInt();

    if (_versionFromMetadata == 1) {
        setupExistingMetadataVersion1(metadata);
        return;
    }
    setupExistingMetadataVersion2(metadata);
}
void FolderMetadata::setupExistingMetadataVersion1(const QByteArray &metadata)
{
    const auto doc = QJsonDocument::fromJson(metadata);

    const auto metaDataStr = metadataStringFromOCsDocument(doc);

    const auto metaDataDoc = QJsonDocument::fromJson(metaDataStr.toLocal8Bit());
    const auto metadataObj = metaDataDoc.object()[metadataJsonKey].toObject();
    const auto metadataKeys = metadataObj[metadataKeysKey].toObject();

    if (metadataKeys.isEmpty()) {
        if (_metadataKey.isEmpty()) {
            qCDebug(lcCseMetadata()) << "Could not setup existing metadata with missing metadataKeys!";
            return;
        }
    }

    {
        const auto currB64Pass = metadataKeys.value(metadataKeys.keys().last()).toString().toLocal8Bit();
        const auto b64DecryptedKey = decryptData(currB64Pass);
        _metadataKey = QByteArray::fromBase64(b64DecryptedKey);
    }

    // Iterate over the document to store the keys. I'm unsure that the keys are in order,
    // perhaps it's better to store a map instead of a vector, perhaps this just doesn't matter.
    for (auto it = metadataKeys.constBegin(), end = metadataKeys.constEnd(); it != end; it++) {
        QByteArray currB64Pass = it.value().toString().toLocal8Bit();
        /*
         * We have to base64 decode the metadatakey here. This was a misunderstanding in the RFC
         * Now we should be compatible with Android and IOS. Maybe we can fix it later.
         */
        QByteArray b64DecryptedKey = decryptData(currB64Pass);
        if (b64DecryptedKey.isEmpty()) {
            qCDebug(lcCseMetadata()) << "Could not decrypt metadata for key" << it.key();
            continue;
        }

        QByteArray decryptedKey = QByteArray::fromBase64(b64DecryptedKey);
        _metadataKeys.insert(it.key().toInt(), decryptedKey);
    }

    QJsonDocument debugHelper;
    debugHelper.setObject(metadataKeys);
    qCDebug(lcCseMetadata()) << "Keys: " << debugHelper.toJson(QJsonDocument::Compact);

    if (_metadataKey.isEmpty()) {
        qCDebug(lcCseMetadata()) << "Could not decrypt metadata key!";
        return;
    }

    _fileDropEncrypted = metaDataDoc.object().value(filedropKey).toObject().value(cipherTextKey).toString().toLocal8Bit();

    const auto sharing = metadataObj[sharingKey].toString().toLocal8Bit();
    const auto files = metaDataDoc.object()[filesKey].toObject();

    // Cool, We actually have the key, we can decrypt the rest of the metadata.
    qCDebug(lcCseMetadata()) << "Sharing: " << sharing;
    if (sharing.size()) {
        auto sharingDecrypted = decryptJsonObject(sharing, _metadataKey);
        qCDebug(lcCseMetadata()) << "Sharing Decrypted" << sharingDecrypted;

        // Sharing is also a JSON object, so extract it and populate.
        auto sharingDoc = QJsonDocument::fromJson(sharingDecrypted);
        auto sharingObj = sharingDoc.object();
        for (auto it = sharingObj.constBegin(), end = sharingObj.constEnd(); it != end; it++) {
            _sharing.push_back({it.key(), it.value().toString()});
        }
    } else {
        qCDebug(lcCseMetadata()) << "Skipping sharing section since it is empty";
    }

    for (auto it = files.constBegin(), end = files.constEnd(); it != end; it++) {
        const auto parsedEncryptedFile = parseFileAndFolderFromJson(it.key(), it.value());
        if (!parsedEncryptedFile.originalFilename.isEmpty()) {
            _files.push_back(parsedEncryptedFile);
        }
    }
}

void FolderMetadata::setupExistingMetadataVersion2(const QByteArray &metadata)
{
    const auto doc = QJsonDocument::fromJson(metadata);

    const auto metaDataStr = metadataStringFromOCsDocument(doc);

    const auto metaDataDoc = QJsonDocument::fromJson(metaDataStr.toLocal8Bit());
    const auto metadataObj = metaDataDoc.object()[metadataJsonKey].toObject();
    const auto folderUsers = metaDataDoc[usersKey].toArray();

    if (folderUsers.isEmpty()) {
        if (_topLevelFolderMetadata) {
            _metadataKey = _topLevelFolderMetadata->metadataKey();
        }
    }

    QJsonDocument debugHelper;
    debugHelper.setArray(folderUsers);
    qCDebug(lcCseMetadata()) << "users: " << debugHelper.toJson(QJsonDocument::Compact);

    for (auto it = folderUsers.constBegin(); it != folderUsers.constEnd(); ++it) {
        const auto folderUserObject = it->toObject();
        const auto userId = folderUserObject.value(usersUserIdKey).toString();
        FolderUser folderUser;
        folderUser.userId = userId;
        folderUser.certificatePem = folderUserObject.value(usersCertificateKey).toString().toUtf8();
        folderUser.encryptedMetadataKey = folderUserObject.value(usersEncryptedMetadataKey).toString().toUtf8();
        folderUser.encryptedFiledropKey = folderUserObject.value(usersEncryptedFiledropKey).toString().toUtf8();
        _folderUsers[userId] = folderUser;
    }

    if (_folderUsers.contains(_account->davUser())) {
        const auto currentFolderUser = _folderUsers.value(_account->davUser());

        const auto currentFolderUserEncryptedMetadataKey = currentFolderUser.encryptedMetadataKey;
        _metadataKey = QByteArray::fromBase64(decryptData(currentFolderUserEncryptedMetadataKey));

        const auto currentFolderUserEncryptedFiledropKey = currentFolderUser.encryptedFiledropKey;
        _fileDropKey = QByteArray::fromBase64(decryptData(currentFolderUserEncryptedFiledropKey));
    }

    if (_metadataKey.isEmpty()) {
        qCDebug(lcCseMetadata()) << "Could not decrypt metadata key!";
        return;
    }

    _fileDropEncrypted = metaDataDoc.object().value(filedropKey).toObject().value(cipherTextKey).toString().toLocal8Bit();

    _metadataNonce = QByteArray::fromBase64(metadataObj[nonceKey].toString().toLocal8Bit());
    const auto authenticationTag = QByteArray::fromBase64(metadataObj[authenticationTagKey].toString().toLocal8Bit());
    const auto cipherText = metadataObj[cipherTextKey].toString().toLocal8Bit();
    const auto cipherTextDecrypted = base64DecodeDecryptAndGzipUnZip(_metadataKey, cipherText, _metadataNonce);
    const auto cipherTextDocument = QJsonDocument::fromJson(cipherTextDecrypted);
    const auto keyChecksums = cipherTextDocument[keyChecksumsKey].toArray();
    for (auto it = keyChecksums.constBegin(); it != keyChecksums.constEnd(); ++it) {
        const auto keyChecksum = it->toVariant().toString().toUtf8();
        if (!keyChecksum.isEmpty()) {
            _keyChecksums.insert(keyChecksum);
        }
    }

    if (_topLevelFolderMetadata) {
        if (!_topLevelFolderMetadata->verifyMetadataKey(_metadataKey)) {
            qCDebug(lcCseMetadata()) << "Could not verify metadataKey!";
            return;
        }
    } else {
        if (!verifyMetadataKey(_metadataKey)) {
            qCDebug(lcCseMetadata()) << "Could not verify metadataKey!";
            return;
        }
    }

    if (cipherTextDecrypted.isEmpty()) {
        qCDebug(lcCseMetadata()) << "Could not decrypt metadata key!";
        return;
    }

    const auto sharing = cipherTextDocument[sharingKey].toString().toLocal8Bit();
    const auto files = cipherTextDocument.object()[filesKey].toObject();
    const auto folders = cipherTextDocument.object()[foldersKey].toObject();

    // Cool, We actually have the key, we can decrypt the rest of the metadata.
    qCDebug(lcCseMetadata()) << "Sharing: " << sharing;
    if (sharing.size()) {
        auto sharingDecrypted = decryptJsonObject(sharing, _metadataKey);
        qCDebug(lcCseMetadata()) << "Sharing Decrypted" << sharingDecrypted;

        // Sharing is also a JSON object, so extract it and populate.
        auto sharingDoc = QJsonDocument::fromJson(sharingDecrypted);
        auto sharingObj = sharingDoc.object();
        for (auto it = sharingObj.constBegin(), end = sharingObj.constEnd(); it != end; it++) {
            _sharing.push_back({it.key(), it.value().toString()});
        }
    } else {
        qCDebug(lcCseMetadata()) << "Skipping sharing section since it is empty";
    }

    for (auto it = files.constBegin(), end = files.constEnd(); it != end; it++) {
        const auto parsedEncryptedFile = parseFileAndFolderFromJson(it.key(), it.value());
        if (!parsedEncryptedFile.originalFilename.isEmpty()) {
            _files.push_back(parsedEncryptedFile);
        }
    }

    for (auto it = folders.constBegin(), end = folders.constEnd(); it != end; it++) {
        const auto parsedEncryptedFolder = parseFileAndFolderFromJson(it.key(), it.value());
        if (!parsedEncryptedFolder.originalFilename.isEmpty()) {
            _files.push_back(parsedEncryptedFolder);
        }
    }
}

void FolderMetadata::emitSetupComplete()
{
    QTimer::singleShot(0, this, [this]() {
        emit setupComplete();
    });
}

// RSA/ECB/OAEPWithSHA-256AndMGF1Padding using private / public key.
QByteArray FolderMetadata::encryptData(const QByteArray& data) const
{
    return encryptData(data, _account->e2e()->_publicKey);
}

QByteArray FolderMetadata::encryptData(const QByteArray &data, const QSslKey key) const
{
    ClientSideEncryption::Bio publicKeyBio;
    const auto publicKeyPem = key.toPem();
    BIO_write(publicKeyBio, publicKeyPem.constData(), publicKeyPem.size());
    const auto publicKey = ClientSideEncryption::PKey::readPublicKey(publicKeyBio);

    // The metadata key is binary so base64 encode it first
    return EncryptionHelper::encryptStringAsymmetric(publicKey, data.toBase64());
}

QByteArray FolderMetadata::decryptData(const QByteArray &data) const
{
    ClientSideEncryption::Bio privateKeyBio;
    QByteArray privateKeyPem = _account->e2e()->_privateKey;
    
    BIO_write(privateKeyBio, privateKeyPem.constData(), privateKeyPem.size());
    auto key = ClientSideEncryption::PKey::readPrivateKey(privateKeyBio);

    // Also base64 decode the result
    QByteArray decryptResult = EncryptionHelper::decryptStringAsymmetric(key, QByteArray::fromBase64(data));

    if (decryptResult.isEmpty())
    {
      qCDebug(lcCseMetadata()) << "ERROR. Could not decrypt the metadata key";
      return {};
    }
    return QByteArray::fromBase64(decryptResult);
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

[[nodiscard]] QByteArray FolderMetadata::encryptCipherText(const QByteArray &cipherText, const QByteArray &pass, const QByteArray &initializationVector, QByteArray &returnTag) const
{
    auto encrypted = FolderMetadata::gZipEncryptAndBase64Encode(pass, cipherText, initializationVector, returnTag);
    auto decrypted = FolderMetadata::base64DecodeDecryptAndGzipUnZip(pass, encrypted, initializationVector);
    return FolderMetadata::gZipEncryptAndBase64Encode(pass, cipherText, initializationVector, returnTag);
}

[[nodiscard]] QByteArray FolderMetadata::decryptCipherText(const QByteArray &encryptedCipherText, const QByteArray &pass, const QByteArray &initializationVector) const
{
    return FolderMetadata::base64DecodeDecryptAndGzipUnZip(pass, encryptedCipherText, initializationVector);
}

bool FolderMetadata::isMetadataSetup() const
{
    const auto result = !_metadataKey.isEmpty() || !_metadataKeys.isEmpty();
    if (!result) {
        int a = 5;
        a = 6;
    }
    return result;
}

EncryptedFile FolderMetadata::parseFileAndFolderFromJson(const QString &encryptedFilename, const QJsonValue &fileJSON) const
{
    const auto fileObj = fileJSON.toObject();
    // Decrypt encrypted part
    const auto encryptedFile = fileObj["encrypted"].toString().toLocal8Bit();
    const auto decryptedFile = !_metadataKey.isEmpty() ? decryptJsonObject(encryptedFile, _metadataKey) : QByteArray{};
    const auto decryptedFileDoc = QJsonDocument::fromJson(decryptedFile);
    const auto decryptedFileObj = decryptedFileDoc.object();
    if (decryptedFileObj["filename"].toString().isEmpty()) {
        qCDebug(lcCseMetadata()) << "decrypted metadata" << decryptedFileDoc.toJson(QJsonDocument::Indented);
        qCWarning(lcCseMetadata()) << "skipping encrypted file" << encryptedFilename << "metadata has an empty file name";
        return {};
    }
    
    EncryptedFile file;
    file.encryptedFilename = encryptedFilename;
    file.metadataKey = fileObj[metadataKeyKey].toInt();
    file.authenticationTag = QByteArray::fromBase64(fileObj[authenticationTagKey].toString().toLocal8Bit());
    file.initializationVector = QByteArray::fromBase64(fileObj[initializationVectorKey].toString().toLocal8Bit());
    file.originalFilename = decryptedFileObj["filename"].toString();
    file.encryptionKey = QByteArray::fromBase64(decryptedFileObj["key"].toString().toLocal8Bit());
    file.mimetype = decryptedFileObj["mimetype"].toString().toLocal8Bit();
    file.fileVersion = decryptedFileObj[versionKey].toInt();

    // In case we wrongly stored "inode/directory" we try to recover from it
    if (file.mimetype == QByteArrayLiteral("inode/directory")) {
        file.mimetype = QByteArrayLiteral("httpd/unix-directory");
    }

    return file;
}

QJsonObject FolderMetadata::encryptedFileToJsonObject(const EncryptedFile *encryptedFile, const QByteArray &metadataKey) const
{
    QJsonObject encrypted;
    encrypted.insert("key", QString(encryptedFile->encryptionKey.toBase64()));
    encrypted.insert("filename", encryptedFile->originalFilename);
    encrypted.insert("mimetype", QString(encryptedFile->mimetype));
    encrypted.insert(versionKey, encryptedFile->fileVersion);

    QJsonDocument encryptedDoc;
    encryptedDoc.setObject(encrypted);

    const QString encryptedEncrypted = encryptJsonObject(encryptedDoc.toJson(QJsonDocument::Compact), metadataKey);
    if (encryptedEncrypted.isEmpty()) {
        qCDebug(lcCseMetadata()) << "Metadata generation failed!";
        return {};
    }

    QJsonObject file;
    file.insert("encrypted", encryptedEncrypted);
    file.insert(initializationVectorKey, QString(encryptedFile->initializationVector.toBase64()));
    file.insert(authenticationTagKey, QString(encryptedFile->authenticationTag.toBase64()));

    return file;
}

bool FolderMetadata::isTopLevelFolder() const
{
    return _topLevelFolderPath == QStringLiteral("/");
}

QByteArray FolderMetadata::gZipEncryptAndBase64Encode(const QByteArray &key, const QByteArray &inputData, const QByteArray &iv, QByteArray &returnTag)
{
    QBuffer gZipBuffer;
    auto gZipCompressionDevice = KCompressionDevice(&gZipBuffer, false, KCompressionDevice::GZip);
    if (!gZipCompressionDevice.open(QIODevice::WriteOnly)) {
        return {};
    }
    const auto bytesWritten = gZipCompressionDevice.write(inputData);
    gZipCompressionDevice.close();
    if (bytesWritten < 0) {
        return {};
    }

    if (!gZipBuffer.open(QIODevice::ReadOnly)) {
        return {};
    }

    QByteArray outputData;
    returnTag.clear();
    const auto gZippedAndNotEncrypted = gZipBuffer.readAll();
    EncryptionHelper::dataEncryption(key, iv, gZippedAndNotEncrypted, outputData, returnTag);
    gZipBuffer.close();
    return outputData.toBase64();
}

QByteArray FolderMetadata::base64DecodeDecryptAndGzipUnZip(const QByteArray &key, const QByteArray &inputData, const QByteArray &iv)
{
    QByteArray decryptedAndGzipped;
    EncryptionHelper::dataDecryption(key, iv, QByteArray::fromBase64(inputData), decryptedAndGzipped);

    QBuffer gZipBuffer;
    if (!gZipBuffer.open(QIODevice::WriteOnly)) {
        return {};
    }
    const auto bytesWritten = gZipBuffer.write(decryptedAndGzipped);
    gZipBuffer.close();
    if (bytesWritten < 0) {
        return {};
    }

    auto gZipUnCompressionDevice = KCompressionDevice(&gZipBuffer, false, KCompressionDevice::GZip);
    if (!gZipUnCompressionDevice.open(QIODevice::ReadOnly)) {
        return {};
    }

    const auto decryptedAndUnGzipped = gZipUnCompressionDevice.readAll();
    gZipUnCompressionDevice.close();

    return decryptedAndUnGzipped;
}

const QByteArray &FolderMetadata::metadataKey() const
{
    return _metadataKey;
}

const QSet<QByteArray>& FolderMetadata::keyChecksums() const
{
    return _keyChecksums;
}

int FolderMetadata::versionFromMetadata() const
{
    return _versionFromMetadata;
}

void FolderMetadata::setupEmptyMetadataV2()
{
    qCDebug(lcCseMetadata()) << "Setting up empty metadata v2";
    if (_topLevelFolderMetadata) {
        _metadataKey = _topLevelFolderMetadata->metadataKey();
        _keyChecksums = _topLevelFolderMetadata->keyChecksums();
    }

    if (_metadataKey.isEmpty()) {
        createNewMetadataKey();
    }
    
    if (!_topLevelFolderMetadata && _topLevelFolderPath.isEmpty() || isTopLevelFolder()) {
        FolderUser folderUser;
        folderUser.userId = _account->davUser();
        folderUser.certificatePem = _account->e2e()->_certificate.toPem();
        folderUser.encryptedMetadataKey = encryptData(_metadataKey.toBase64());

        _folderUsers[_account->davUser()] = folderUser;
    }

    QString publicKey = _account->e2e()->_publicKey.toPem().toBase64();
    QString displayName = _account->displayName();

    _sharing.append({displayName, publicKey});
}

void FolderMetadata::setupEmptyMetadataV1()
{
    qCDebug(lcCseMetadata) << "Settint up empty metadata v1";
    QByteArray newMetadataPass = EncryptionHelper::generateRandom(16);
    _metadataKeys.insert(0, newMetadataPass);

    QString publicKey = _account->e2e()->_publicKey.toPem().toBase64();
    QString displayName = _account->displayName();

    _sharing.append({displayName, publicKey});
}

void FolderMetadata::encryptMetadata()
{
    qCDebug(lcCseMetadata()) << "Generating metadata";

    if (!isTopLevelFolder() && versionFromMetadata() != 1) {
        // up-to-date top level folder metadata is required to proceed (for v2 folders or unknown version folders)
        _isEncryptionRequested = true;
        startFetchTopLevelFolderMetadata();
        return;
    }

    handleEncryptionRequest();
}

void FolderMetadata::setMetadataKeyOverride(const QByteArray &metadataKeyOverride)
{
    _metadataKeyOverride = metadataKeyOverride;
}

void FolderMetadata::handleEncryptionRequest()
{
    _isEncryptionRequested = false;
    if ((isTopLevelFolder() && versionFromMetadata() == 1) || _topLevelFolderMetadata && _topLevelFolderMetadata->versionFromMetadata() == 1) {
        handleEncryptionRequestV1();
        return;
    }

    handleEncryptionRequestV2();
}

void FolderMetadata::handleEncryptionRequestV2()
{
    const auto metadataKeyForEncryption = !_metadataKeyOverride.isEmpty() ? _metadataKeyOverride : _metadataKey;
    if (!_metadataKeyOverride.isEmpty()) {
        _metadataKey = _metadataKeyOverride;
    }
    if (metadataKeyForEncryption.isEmpty()) {
        qCDebug(lcCseMetadata()) << "Metadata generation failed! Empty metadata key!";
        QTimer::singleShot(0, this, [this]() {
            emit encryptionFinished({});
        });
        return;
    }

    QJsonArray folderUsers;
    if (isTopLevelFolder()) {
        for (auto it = _folderUsers.constBegin(), end = _folderUsers.constEnd(); it != end; ++it) {
            const auto folderUser = it.value();

            const QJsonObject folderUserJson{
                {usersUserIdKey, folderUser.userId},
                {usersCertificateKey, QJsonValue::fromVariant(folderUser.certificatePem)},
                {usersEncryptedMetadataKey, QJsonValue::fromVariant(folderUser.encryptedMetadataKey)},
                {usersEncryptedFiledropKey, QJsonValue::fromVariant(folderUser.encryptedFiledropKey)}
            };
            folderUsers.push_back(folderUserJson);
        }
    }

    
    if (isTopLevelFolder() && folderUsers.isEmpty()) {
        qCDebug(lcCseMetadata) << "Empty folderUsers while shouldn't be empty!";
    }

    QJsonObject files;
    QJsonObject folders;
    for (auto it = _files.constBegin(), end = _files.constEnd(); it != end; it++) {
        const auto file = encryptedFileToJsonObject(it, _metadataKey);
        if (file.isEmpty()) {
            QTimer::singleShot(0, this, [this]() {
                emit encryptionFinished({});
            });
            return;
        }
        if (it->mimetype == QByteArrayLiteral("httpd/unix-directory") || it->mimetype == QByteArrayLiteral("inode/directory")) {
            folders.insert(it->encryptedFilename, file);
        } else {
            files.insert(it->encryptedFilename, file);
        }
    }

    QJsonArray keyChecksums;
    if (isTopLevelFolder()) {
        for (auto it = _keyChecksums.constBegin(), end = _keyChecksums.constEnd(); it != end; ++it) {
            keyChecksums.push_back(QJsonValue::fromVariant(*it));
        }
    }

    QJsonObject cipherText = {
        {filesKey, files},
        {foldersKey, folders}
    };

    if (!keyChecksums.isEmpty()) {
        cipherText.insert(keyChecksumsKey, keyChecksums);
    }

    const QJsonDocument cipherTextDoc(cipherText);

    QByteArray authenticationTag;
    const auto initializationVector = EncryptionHelper::generateRandom(16);
    const auto encryptedCipherText = encryptCipherText(cipherTextDoc.toJson(QJsonDocument::Compact), _metadataKey, initializationVector, authenticationTag);
    const QJsonObject metadata{
        {cipherTextKey, QJsonValue::fromVariant(encryptedCipherText)},
        {nonceKey, QJsonValue::fromVariant(initializationVector.toBase64())},
        {authenticationTagKey, QJsonValue::fromVariant(authenticationTag.toBase64())}
    };

    QJsonObject metaObject = {{metadataJsonKey, metadata}, {versionKey, 2}};

    if (!folderUsers.isEmpty()) {
        metaObject.insert(usersKey, folderUsers);
    }

    QJsonDocument internalMetadata;
    internalMetadata.setObject(metaObject);

    auto jsonString = internalMetadata.toJson();

    auto newdoc = QJsonDocument::fromJson(jsonString);
    auto jsonObj = newdoc.object();

    auto usersFromDoc = jsonObj.value(usersKey).toArray();

    if (usersFromDoc.isEmpty()) {
        int a = 5;
        a = 6;
    }

    QTimer::singleShot(0, this, [this, internalMetadata]() {
        emit encryptionFinished(internalMetadata.toJson());
    });
}

void FolderMetadata::handleEncryptionRequestV1()
{
    qCDebug(lcCseMetadata) << "Generating metadata for v1 encrypted folder";

    if (_metadataKey.isEmpty() || _metadataKeys.isEmpty()) {
        qCDebug(lcCseMetadata) << "Metadata generation failed! Empty metadata key!";
        QTimer::singleShot(0, this, [this]() {
            emit encryptionFinished({});
        });
        return;
    }

    QJsonObject metadataKeys;
    for (auto it = _metadataKeys.constBegin(), end = _metadataKeys.constEnd(); it != end; ++it) {
        const auto encryptedKey = encryptData(_metadataKey.toBase64());
        metadataKeys.insert(QString::number(it.key()), QString(encryptedKey));
    }

    const QJsonObject metadata{
        {metadataKeysKey, metadataKeys},
     // {sharingKey, sharingEncrypted},
        {versionKey, 1}
    };

    QJsonObject files;
    for (auto it = _files.constBegin(); it != _files.constEnd(); ++it) {
        const auto file = encryptedFileToJsonObject(it, _metadataKey);
        if (file.isEmpty()) {
            QTimer::singleShot(0, this, [this]() {
                emit encryptionFinished({});
            });
            return;
        }
        files.insert(it->encryptedFilename, file);
    }

    const QJsonObject metaObject{
        {metadataJsonKey, metadata},
        {filesKey, files}
    };

    QJsonDocument internalMetadata;
    internalMetadata.setObject(metaObject);
    QTimer::singleShot(0, this, [this, internalMetadata]() {
        emit encryptionFinished(internalMetadata.toJson());
    });
}

void FolderMetadata::addEncryptedFile(const EncryptedFile &f) {

    for (int i = 0; i < _files.size(); i++) {
        if (_files.at(i).originalFilename == f.originalFilename) {
            _files.removeAt(i);
            break;
        }
    }

    _files.append(f);
}

void FolderMetadata::removeEncryptedFile(const EncryptedFile &f)
{
    for (int i = 0; i < _files.size(); i++) {
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

QVector<EncryptedFile> FolderMetadata::files() const {
    return _files;
}

bool FolderMetadata::isFileDropPresent() const
{
    return _fileDropEncrypted.size() > 0;
}

bool FolderMetadata::moveFromFileDropToFiles()
{
    if (_fileDropEncrypted.isEmpty() || _metadataKey.isEmpty() || _metadataNonce.isEmpty()) {
        return false;
    }

    const auto cipherTextDecrypted = base64DecodeDecryptAndGzipUnZip(_metadataKey, _fileDropEncrypted, _metadataNonce);
    const auto cipherTextDocument = QJsonDocument::fromJson(cipherTextDecrypted);

    const auto files = cipherTextDocument.object()[filesKey].toObject();
    const auto folders = cipherTextDocument.object()[foldersKey].toObject();

    for (auto it = files.constBegin(); it != files.constEnd(); ++it) {
        const auto parsedEncryptedFile = parseFileAndFolderFromJson(it.key(), it.value());
        if (!parsedEncryptedFile.originalFilename.isEmpty()) {
            _files.push_back(parsedEncryptedFile);
        }
    }

    for (auto it = folders.constBegin(); it != folders.constEnd(); ++it) {
        const auto parsedEncryptedFile = parseFileAndFolderFromJson(it.key(), it.value());
        if (!parsedEncryptedFile.originalFilename.isEmpty()) {
            _files.push_back(parsedEncryptedFile);
        }
    }

    return true;
}

const QByteArray &FolderMetadata::fileDrop() const
{
    return _fileDropEncrypted;
}

void FolderMetadata::startFetchTopLevelFolderMetadata()
{
    const auto job = new LsColJob(_account, _topLevelFolderPath, this);
    job->setProperties({"resourcetype", "http://owncloud.org/ns:fileid"});
    connect(job, &LsColJob::directoryListingSubfolders, this, &FolderMetadata::topLevelFolderEncryptedIdReceived);
    connect(job, &LsColJob::finishedWithError, this, &FolderMetadata::topLevelFolderEncryptedIdError);
    job->start();
}

void FolderMetadata::fetchTopLevelFolderMetadata(const QByteArray &folderId)
{
    const auto getMetadataJob = new GetMetadataApiJob(_account, folderId);
    connect(getMetadataJob, &GetMetadataApiJob::jsonReceived, this, &FolderMetadata::topLevelFolderEncryptedMetadataReceived);
    connect(getMetadataJob, &GetMetadataApiJob::error, this, &FolderMetadata::topLevelFolderEncryptedMetadataError);
    getMetadataJob->start();
}

void FolderMetadata::topLevelFolderEncryptedIdReceived(const QStringList &list)
{
    const auto job = qobject_cast<LsColJob *>(sender());
    Q_ASSERT(job);
    if (!job || job->_folderInfos.isEmpty()) {
        topLevelFolderEncryptedMetadataReceived({}, 404);
        return;
    }
    fetchTopLevelFolderMetadata(job->_folderInfos.value(list.first()).fileId);
}

void FolderMetadata::topLevelFolderEncryptedMetadataError(const QByteArray &fileId, int httpReturnCode)
{
    Q_UNUSED(fileId);
    Q_UNUSED(httpReturnCode);
    topLevelFolderEncryptedMetadataReceived({}, httpReturnCode);
}

void FolderMetadata::topLevelFolderEncryptedMetadataReceived(const QJsonDocument &json, int statusCode)
{
    if (!json.isEmpty()) {
        _topLevelFolderMetadata.reset(new FolderMetadata(_account, json.toJson(QJsonDocument::Compact), QStringLiteral("/")));
        if (_topLevelFolderMetadata->versionFromMetadata() == -1 || _topLevelFolderMetadata->versionFromMetadata() > 1) {
            _metadataKey = _topLevelFolderMetadata->metadataKey();
            _keyChecksums = _topLevelFolderMetadata->keyChecksums();
        }
    }
    if (_isEncryptionRequested) {
        handleEncryptionRequest();
    } else {
        setupMetadata();
    }
}

void FolderMetadata::topLevelFolderEncryptedIdError(QNetworkReply *reply)
{
    topLevelFolderEncryptedMetadataReceived({}, reply ? reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() : 0);
}

bool FolderMetadata::addUser(const QString &userId, const QSslCertificate certificate)
{
    Q_ASSERT(isTopLevelFolder());
    if (!isTopLevelFolder()) {
        qCWarning(lcCseMetadata()) << "Could not add a folder user to a non top level folder.";
        return false;
    }

    const auto certificatePublicKey = certificate.publicKey();
    if (userId.isEmpty() || certificate.isNull() || certificatePublicKey.isNull()) {
        qCWarning(lcCseMetadata()) << "Could not add a folder user. Invalid userId or certificate.";
        return false;
    }

    createNewMetadataKey();
    FolderUser newFolderUser;
    newFolderUser.userId = userId;
    newFolderUser.certificatePem = certificate.toPem();
    newFolderUser.encryptedMetadataKey = encryptData(_metadataKey, certificatePublicKey).toBase64();
    _folderUsers[userId] = newFolderUser;
    updateUsersEncryptedMetadataKey();

    return true;
}

bool FolderMetadata::removeUser(const QString &userId)
{
    Q_ASSERT(isTopLevelFolder());
    if (!isTopLevelFolder()) {
        qCWarning(lcCseMetadata()) << "Could not add remove folder user from a non top level folder.";
        return false;
    }
    Q_ASSERT(!userId.isEmpty());
    if (userId.isEmpty()) {
        qCDebug(lcCseMetadata()) << "Could not remove a folder user. Invalid userId.";
        return false;
    }

    createNewMetadataKey();
    _folderUsers.remove(userId);
    updateUsersEncryptedMetadataKey();

    return true;
}

void FolderMetadata::setTopLevelFolderMetadata(const QSharedPointer<FolderMetadata> &topLevelFolderMetadata)
{
    _topLevelFolderMetadata = topLevelFolderMetadata;
}

void FolderMetadata::updateUsersEncryptedMetadataKey()
{
    Q_ASSERT(isTopLevelFolder());
    if (!isTopLevelFolder()) {
        qCWarning(lcCseMetadata()) << "Could not update folder users in a non top level folder.";
        return;
    }
    Q_ASSERT(!_metadataKey.isEmpty());
    if (_metadataKey.isEmpty()) {
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

        const auto encryptedMetadataKey = encryptData(_metadataKey, certificatePublicKey).toBase64();
        if (encryptedMetadataKey.isEmpty()) {
            qCWarning(lcCseMetadata()) << "Could not update folder users with empty encryptedMetadataKey!";
            continue;
        }

        folderUser.encryptedMetadataKey = encryptedMetadataKey;

        _folderUsers[it.key()] = folderUser;
    }
}

void FolderMetadata::createNewMetadataKey()
{
    if (_topLevelFolderMetadata && (_topLevelFolderMetadata->versionFromMetadata() == -1 || _topLevelFolderMetadata->versionFromMetadata() > 1)) {
        _topLevelFolderMetadata->createNewMetadataKey();
        _metadataKey = _topLevelFolderMetadata->metadataKey();
        _keyChecksums = _topLevelFolderMetadata->keyChecksums();
        return;
    }
    if (!_metadataKey.isEmpty()) {
        const auto existingMd5 = calcMd5(_metadataKey);
        _keyChecksums.remove(existingMd5);
    }
    _metadataKey = EncryptionHelper::generateRandom(16);
    const auto newMd5 = calcMd5(_metadataKey);
    _keyChecksums.insert(newMd5);
}

bool FolderMetadata::verifyMetadataKey(const QByteArray &metadataKey) const
{
    const auto md5MetadataKey = calcMd5(metadataKey);
    return _keyChecksums.contains(md5MetadataKey);
}
}
