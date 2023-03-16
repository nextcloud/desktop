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
                               const QSharedPointer<FolderMetadata> &topLevelFolderMetadata,
                               const QByteArray &metadataKeyForDecryption,
                               QObject *parent)
    : QObject(parent)
    , _account(account)
    , _initialMetadata(metadata)
    , _topLevelFolderPath(topLevelFolderPath)
    , _metadataKeyForDecryption(metadataKeyForDecryption)
    , _topLevelFolderMetadata(topLevelFolderMetadata)
{

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

    if (!isTopLevelFolder() && !_topLevelFolderMetadata && (_versionFromMetadata == -1 || _versionFromMetadata >= 2)) {
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
        _metadataKey = decryptData(QByteArray::fromBase64(currB64Pass));
    }

    // Iterate over the document to store the keys. I'm unsure that the keys are in order,
    // perhaps it's better to store a map instead of a vector, perhaps this just doesn't matter.
    for (auto it = metadataKeys.constBegin(), end = metadataKeys.constEnd(); it != end; it++) {
        QByteArray metadataKeyBase64 = it.value().toString().toLocal8Bit();
        /*
         * We have to base64 decode the metadatakey here. This was a misunderstanding in the RFC
         * Now we should be compatible with Android and IOS. Maybe we can fix it later.
         */
        QByteArray decryptedMetadataKey = decryptData(QByteArray::fromBase64(metadataKeyBase64));
        if (decryptedMetadataKey.isEmpty()) {
            qCDebug(lcCseMetadata()) << "Could not decrypt metadata for key" << it.key();
            continue;
        }

        _metadataKeys.insert(it.key().toInt(), QByteArray::fromBase64(decryptedMetadataKey));
    }

    QJsonDocument debugHelper;
    debugHelper.setObject(metadataKeys);
    qCDebug(lcCseMetadata()) << "Keys: " << debugHelper.toJson(QJsonDocument::Compact);

    if (_metadataKey.isEmpty()) {
        qCDebug(lcCseMetadata()) << "Could not decrypt metadata key!";
        return;
    }

    const auto fileDropObject = metaDataDoc.object().value(filedropKey).toObject();
    _fileDropCipherTextEncryptedAndBase64 = fileDropObject.value(cipherTextKey).toString().toLocal8Bit();
    _fileDropMetadataAuthenticationTag = QByteArray::fromBase64(fileDropObject.value(authenticationTagKey).toString().toLocal8Bit());
    _fileDropMetadataNonce = QByteArray::fromBase64(fileDropObject.value(nonceKey).toString().toLocal8Bit());

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

    const auto fileDropObject = metaDataDoc.object().value(filedropKey).toObject();
    _fileDropCipherTextEncryptedAndBase64 = fileDropObject.value(cipherTextKey).toString().toLocal8Bit();
    _fileDropMetadataAuthenticationTag = QByteArray::fromBase64(fileDropObject.value(authenticationTagKey).toString().toLocal8Bit());
    _fileDropMetadataNonce = QByteArray::fromBase64(fileDropObject.value(nonceKey).toString().toLocal8Bit());

    const auto metadataObj = metaDataDoc.object()[metadataJsonKey].toObject();
    const auto folderUsers = metaDataDoc[usersKey].toArray();

    QJsonDocument debugHelper;
    debugHelper.setArray(folderUsers);
    qCDebug(lcCseMetadata()) << "users: " << debugHelper.toJson(QJsonDocument::Compact);

    for (auto it = folderUsers.constBegin(); it != folderUsers.constEnd(); ++it) {
        const auto folderUserObject = it->toObject();
        const auto userId = folderUserObject.value(usersUserIdKey).toString();
        FolderUser folderUser;
        folderUser.userId = userId;
        folderUser.certificatePem = folderUserObject.value(usersCertificateKey).toString().toUtf8();
        folderUser.encryptedMetadataKey = QByteArray::fromBase64(folderUserObject.value(usersEncryptedMetadataKey).toString().toUtf8());
        folderUser.encryptedFiledropKey = QByteArray::fromBase64(folderUserObject.value(usersEncryptedFiledropKey).toString().toUtf8());
        _folderUsers[userId] = folderUser;
    }

    if (_folderUsers.contains(_account->davUser())) {
        const auto currentFolderUser = _folderUsers.value(_account->davUser());
        _metadataKey = decryptData(currentFolderUser.encryptedMetadataKey);
        _fileDropKey = decryptData(currentFolderUser.encryptedFiledropKey);
    }

    if (metadataKeyForDecryption().isEmpty()) {
        qCDebug(lcCseMetadata()) << "Could not decrypt metadata key!";
        return;
    }

    _metadataNonce = QByteArray::fromBase64(metadataObj[nonceKey].toString().toLocal8Bit());
    const auto authenticationTag = QByteArray::fromBase64(metadataObj[authenticationTagKey].toString().toLocal8Bit());
    const auto cipherTextEncryptedAndBase64 = metadataObj[cipherTextKey].toString().toLocal8Bit();
    const auto cipherTextDecrypted = decryptCipherText(cipherTextEncryptedAndBase64, metadataKeyForDecryption(), _metadataNonce);
    const auto cipherTextDocument = QJsonDocument::fromJson(cipherTextDecrypted);
    const auto keyChecksums = cipherTextDocument[keyChecksumsKey].toArray();
    for (auto it = keyChecksums.constBegin(); it != keyChecksums.constEnd(); ++it) {
        const auto keyChecksum = it->toVariant().toString().toUtf8();
        if (!keyChecksum.isEmpty()) {
            _keyChecksums.insert(keyChecksum);
        }
    }

    if (!verifyMetadataKey(_metadataKey)) {
        qCDebug(lcCseMetadata()) << "Could not verify metadataKey!";
        return;
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
        auto sharingDecrypted = decryptJsonObject(sharing, metadataKeyForDecryption());
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

    for (auto it = folders.constBegin(); it != folders.constEnd(); ++it) {
        const auto folderName = it.value().toString();
        if (!folderName.isEmpty()) {
            EncryptedFile file;
            file.encryptedFilename = it.key();
            file.originalFilename = folderName;
            _files.push_back(file);
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
    return EncryptionHelper::encryptStringAsymmetric(publicKey, data);
}

QByteArray FolderMetadata::decryptData(const QByteArray &data) const
{
    ClientSideEncryption::Bio privateKeyBio;
    QByteArray privateKeyPem = _account->e2e()->_privateKey;
    
    BIO_write(privateKeyBio, privateKeyPem.constData(), privateKeyPem.size());
    auto key = ClientSideEncryption::PKey::readPrivateKey(privateKeyBio);

    // Also base64 decode the result
    const auto decryptResult = EncryptionHelper::decryptStringAsymmetric(key, data);

    if (decryptResult.isEmpty())
    {
      qCDebug(lcCseMetadata()) << "ERROR. Could not decrypt the metadata key";
      return {};
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

[[nodiscard]] QByteArray FolderMetadata::encryptCipherText(const QByteArray &cipherText, const QByteArray &pass, const QByteArray &initializationVector, QByteArray &returnTag) const
{
    return FolderMetadata::gZipEncryptAndBase64Encode(pass, cipherText, initializationVector, returnTag);
}

[[nodiscard]] QByteArray FolderMetadata::decryptCipherText(const QByteArray &encryptedCipherText, const QByteArray &pass, const QByteArray &initializationVector) const
{
    return FolderMetadata::base64DecodeDecryptAndGzipUnZip(pass, encryptedCipherText, initializationVector);
}

bool FolderMetadata::isMetadataSetup() const
{
    return !metadataKeyForDecryption().isEmpty() || !_metadataKeys.isEmpty();
}

EncryptedFile FolderMetadata::parseFileAndFolderFromJson(const QString &encryptedFilename, const QJsonValue &fileJSON) const
{
    const auto fileObj = fileJSON.toObject();
    if (fileObj["filename"].toString().isEmpty()) {
        qCWarning(lcCseMetadata()) << "skipping encrypted file" << encryptedFilename << "metadata has an empty file name";
        return {};
    }
    
    EncryptedFile file;
    file.encryptedFilename = encryptedFilename;
    file.metadataKey = fileObj[metadataKeyKey].toInt();
    file.authenticationTag = QByteArray::fromBase64(fileObj[authenticationTagKey].toString().toLocal8Bit());
    file.initializationVector = QByteArray::fromBase64(fileObj[initializationVectorKey].toString().toLocal8Bit());
    file.originalFilename = fileObj["filename"].toString();
    file.encryptionKey = QByteArray::fromBase64(fileObj["key"].toString().toLocal8Bit());
    file.mimetype = fileObj["mimetype"].toString().toLocal8Bit();
    file.fileVersion = fileObj[versionKey].toInt();

    // In case we wrongly stored "inode/directory" we try to recover from it
    if (file.mimetype == QByteArrayLiteral("inode/directory")) {
        file.mimetype = QByteArrayLiteral("httpd/unix-directory");
    }

    return file;
}

QJsonObject FolderMetadata::convertFileToJsonObject(const EncryptedFile *encryptedFile, const QByteArray &metadataKey) const
{
    QJsonObject file;
    file.insert("key", QString(encryptedFile->encryptionKey.toBase64()));
    file.insert("filename", encryptedFile->originalFilename);
    file.insert("mimetype", QString(encryptedFile->mimetype));
    file.insert(versionKey, encryptedFile->fileVersion);
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
    if (!EncryptionHelper::dataDecryption(key, iv, QByteArray::fromBase64(inputData), decryptedAndGzipped)) {
        qCDebug(lcCseMetadata()) << "Could not decrypt";
        return {};
    }

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
    } else {
        createNewMetadataKey();
    }
    
    if (!_topLevelFolderMetadata && _topLevelFolderPath.isEmpty() || isTopLevelFolder()) {
        FolderUser folderUser;
        folderUser.userId = _account->davUser();
        folderUser.certificatePem = _account->e2e()->_certificate.toPem();
        folderUser.encryptedMetadataKey = encryptData(_metadataKey);

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

QByteArray FolderMetadata::encryptedMetadata()
{
    qCDebug(lcCseMetadata()) << "Generating metadata";
    if ((isTopLevelFolder() && versionFromMetadata() == 1) || _topLevelFolderMetadata && _topLevelFolderMetadata->versionFromMetadata() == 1) {
        return handleEncryptionRequestV1();
    }

    return handleEncryptionRequestV2();
}

QByteArray FolderMetadata::handleEncryptionRequestV2()
{
    if (_metadataKey.isEmpty()) {
        qCDebug(lcCseMetadata()) << "Metadata generation failed! Empty metadata key!";
        return {};
    }

    QJsonArray folderUsers;
    if (isTopLevelFolder()) {
        for (auto it = _folderUsers.constBegin(), end = _folderUsers.constEnd(); it != end; ++it) {
            const auto folderUser = it.value();

            const QJsonObject folderUserJson{
                {usersUserIdKey, folderUser.userId},
                {usersCertificateKey, QJsonValue::fromVariant(folderUser.certificatePem)},
                {usersEncryptedMetadataKey, QJsonValue::fromVariant(folderUser.encryptedMetadataKey.toBase64())},
                {usersEncryptedFiledropKey, QJsonValue::fromVariant(folderUser.encryptedFiledropKey.toBase64())}
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
        const auto file = convertFileToJsonObject(it, _metadataKey);
        if (file.isEmpty()) {
            qCDebug(lcCseMetadata) << "Metadata generation failed for file" << it->encryptedFilename;
            return {};
        }
        const auto isDirectory = it->mimetype.isEmpty() || it->mimetype == QByteArrayLiteral("inode/directory") || it->mimetype == QByteArrayLiteral("httpd/unix-directory");
        if (isDirectory) {
            folders.insert(it->encryptedFilename, it->originalFilename);
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
    const auto encryptedCipherTextBase64 = encryptCipherText(cipherTextDoc.toJson(QJsonDocument::Compact), _metadataKey, initializationVector, authenticationTag);
    const auto decryptedCipherTextBase64 = decryptCipherText(encryptedCipherTextBase64, _metadataKey, initializationVector);
    const QJsonObject metadata{
        {cipherTextKey, QJsonValue::fromVariant(encryptedCipherTextBase64)},
        {nonceKey, QJsonValue::fromVariant(initializationVector.toBase64())},
        {authenticationTagKey, QJsonValue::fromVariant(authenticationTag.toBase64())}
    };

    QJsonObject metaObject = {{metadataJsonKey, metadata}, {versionKey, 2}};

    if (!folderUsers.isEmpty()) {
        metaObject.insert(usersKey, folderUsers);
    }

    if (!_fileDropCipherTextEncryptedAndBase64.isEmpty()) {
        const QJsonObject fileDropMetadata{
            {cipherTextKey, QJsonValue::fromVariant(_fileDropCipherTextEncryptedAndBase64)},
            {nonceKey, QJsonValue::fromVariant(_fileDropMetadataNonce.toBase64())},
            {authenticationTagKey, QJsonValue::fromVariant(_fileDropMetadataAuthenticationTag.toBase64())}
        };
        metaObject.insert(filedropKey, fileDropMetadata);
    }

    QJsonDocument internalMetadata;
    internalMetadata.setObject(metaObject);

    auto jsonString = internalMetadata.toJson();

    if (_topLevelFolderMetadata) {
        int a = 5;
        a = 6;
    }

    return internalMetadata.toJson();
}

QByteArray FolderMetadata::handleEncryptionRequestV1()
{
    qCDebug(lcCseMetadata) << "Generating metadata for v1 encrypted folder";

    if (_metadataKey.isEmpty() || _metadataKeys.isEmpty()) {
        qCDebug(lcCseMetadata) << "Metadata generation failed! Empty metadata key!";
        return {};
    }

    QJsonObject metadataKeys;
    for (auto it = _metadataKeys.constBegin(), end = _metadataKeys.constEnd(); it != end; ++it) {
        const auto encryptedKey = encryptData(it.value()).toBase64();
        metadataKeys.insert(QString::number(it.key()), QString(encryptedKey));
    }

    const QJsonObject metadata{
        {metadataKeysKey, metadataKeys},
     // {sharingKey, sharingEncrypted},
        {versionKey, 1}
    };

    QJsonObject files;
    for (auto it = _files.constBegin(); it != _files.constEnd(); ++it) {
        const auto file = convertFileToJsonObject(it, _metadataKey);
        if (file.isEmpty()) {
            qCDebug(lcCseMetadata) << "Metadata generation failed for file" << it->encryptedFilename;
            return {};
        }
        files.insert(it->encryptedFilename, file);
    }

    const QJsonObject metaObject{
        {metadataJsonKey, metadata},
        {filesKey, files}
    };

    QJsonDocument internalMetadata;
    internalMetadata.setObject(metaObject);
    
    return internalMetadata.toJson();
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

QByteArray FolderMetadata::metadataKeyForDecryption() const
{
    return !_metadataKeyForDecryption.isEmpty() ? _metadataKeyForDecryption : _metadataKey;
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
    return !_fileDropCipherTextEncryptedAndBase64.isEmpty();
}

bool FolderMetadata::moveFromFileDropToFiles()
{
    if (_fileDropCipherTextEncryptedAndBase64.isEmpty() || _metadataKey.isEmpty() || _metadataNonce.isEmpty()) {
        return false;
    }

    const auto cipherTextDecrypted = decryptCipherText(_fileDropCipherTextEncryptedAndBase64, _metadataKey, _metadataNonce);
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
        const auto folderName = it.value().toString();
        if (!folderName.isEmpty()) {
            EncryptedFile file;
            file.encryptedFilename = it.key();
            file.originalFilename = folderName;
        }
    }

    _fileDropCipherTextEncryptedAndBase64.clear();

    return true;
}

const QByteArray &FolderMetadata::fileDrop() const
{
    return _fileDropCipherTextEncryptedAndBase64;
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
        connect(_topLevelFolderMetadata.data(), &FolderMetadata::setupComplete, this, [this]() {
            if (_topLevelFolderMetadata->versionFromMetadata() == -1 || _topLevelFolderMetadata->versionFromMetadata() > 1) {
                _metadataKey = _topLevelFolderMetadata->metadataKey();
                _keyChecksums = _topLevelFolderMetadata->keyChecksums();
            }

            setupMetadata();
        });
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
    newFolderUser.encryptedMetadataKey = encryptData(_metadataKey, certificatePublicKey);
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
    if (!_topLevelFolderMetadata) {
        return;
    }
    _metadataKey = _topLevelFolderMetadata->metadataKey();
    _keyChecksums = _topLevelFolderMetadata->keyChecksums();
}

void FolderMetadata::setMetadataKeyForDecryption(const QByteArray &metadataKeyForDecryption)
{
    _metadataKeyForDecryption = metadataKeyForDecryption;
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

        const auto encryptedMetadataKey = encryptData(_metadataKey, certificatePublicKey);
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
    if (!isTopLevelFolder()) {
        return;
    }
    if (!_metadataKey.isEmpty()) {
        const auto existingHash = calcSha256(_metadataKey);
        _keyChecksums.remove(existingHash);
    }
    _metadataKey = EncryptionHelper::generateRandom(16);
    const auto newHash = calcSha256(_metadataKey);
    _keyChecksums.insert(newHash);
}

bool FolderMetadata::verifyMetadataKey(const QByteArray &metadataKey) const
{
    if (_versionFromMetadata < 2) {
        return true;
    }
    const auto metadataKeyHash = calcSha256(metadataKey);
    //return _keyChecksums.contains(metadataKeyHash);
    return true;
}
}
