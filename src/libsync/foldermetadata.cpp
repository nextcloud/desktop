#include "account.h"
#include "foldermetadata.h"
#include "clientsideencryption.h"
#include "clientsideencryptionjobs.h"
#include <common/checksums.h>

#include <QJsonArray>
#include <QJsonDocument>

namespace OCC
{
Q_LOGGING_CATEGORY(lcCseMetadata, "nextcloud.metadata", QtInfoMsg)

namespace
{
constexpr auto keyChecksumsKey = "keyChecksums";
constexpr auto metadataJsonKey = "metadata";
constexpr auto metadataKeyKey = "metadataKey";
constexpr auto metadataKeysKey = "metadataKeys";
constexpr auto usersKey = "users";
constexpr auto usersUserIdKey = "userId";
constexpr auto usersCertificateKey = "certificate";
constexpr auto usersEncryptedMetadataKey = "encryptedMetadataKey";
constexpr auto versionKey = "version";
}

FolderMetadata::FolderMetadata(AccountPtr account,
                               const QByteArray &metadata,
                               int statusCode,
                               const QMap<QString, QSharedPointer<FolderMetadata>> &topLevelFolders,
                               const QString &remotePath,
                               SyncJournalDb *journal,
                               QObject *parent)
    : QObject(parent)
    , _account(account)
    , _initialMetadata(metadata)
    , _initialStatusCode(statusCode)
    , _journal(journal)
{
    const auto remotePathSplit = remotePath.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    _topLevelFolderPath = remotePathSplit.size() > 1 ? remotePathSplit.first() + QStringLiteral("/") : QStringLiteral("/");
    _topLevelFolderMetadata = topLevelFolders.value(_topLevelFolderPath, {});
    Q_ASSERT(_topLevelFolderMetadata || (_topLevelFolderPath.isEmpty() || _topLevelFolderPath == QStringLiteral("/")) || _journal);
    QJsonDocument doc = QJsonDocument::fromJson(metadata);
    qCInfo(lcCseMetadata()) << doc.toJson(QJsonDocument::Compact);

    // The metadata is being retrieved as a string stored in a json.
    // This *seems* to be broken but the RFC doesn't explicits how it wants.
    // I'm currently unsure if this is error on my side or in the server implementation.
    // And because inside of the meta-data there's an object called metadata, without '-'
    // make it really different.

    QString metaDataStr = doc.object()["ocs"].toObject()["data"].toObject()["meta-data"].toString();

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

    if ((_versionFromMetadata == -1 || _versionFromMetadata > 1) && !_topLevelFolderMetadata && !_topLevelFolderPath.isEmpty() && _topLevelFolderPath != QStringLiteral("/") && folderUsers.isEmpty()
        && metadataKeys.isEmpty()) {
        fetchTopLevelFolderEncryptedId();
    } else {
        setupMetadata();
    }
}

void FolderMetadata::setupMetadata()
{
    if (_initialMetadata.isEmpty() || _initialStatusCode == 404) {
        qCInfo(lcCseMetadata()) << "Setupping Empty Metadata";
        setupEmptyMetadata();
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

    const auto metaDataStr = doc.object()["ocs"].toObject()["data"].toObject()["meta-data"].toString();

    const auto metaDataDoc = QJsonDocument::fromJson(metaDataStr.toLocal8Bit());
    const auto metadataObj = metaDataDoc.object()[metadataJsonKey].toObject();
    _versionFromMetadata = metadataObj.contains(versionKey) ? metadataObj[versionKey].toInt() : metaDataDoc.object()[versionKey].toInt();

    if (_versionFromMetadata >= 2) {
        setupExistingMetadataVersion2(metadata);
    } else {
        setupExistingMetadataVersion1(metadata);
    }
}
void FolderMetadata::setupExistingMetadataVersion1(const QByteArray &metadata)
{
    const auto doc = QJsonDocument::fromJson(metadata);

    const auto metaDataStr = doc.object()["ocs"].toObject()["data"].toObject()["meta-data"].toString();

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

    _fileDrop = metaDataDoc.object().value("filedrop").toObject();

    const auto sharing = metadataObj["sharing"].toString().toLocal8Bit();
    const auto files = metaDataDoc.object()["files"].toObject();

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

    const auto metaDataStr = doc.object()["ocs"].toObject()["data"].toObject()["meta-data"].toString();

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
        const auto folderUser = it->toObject();
        const auto userId = folderUser.value(usersUserIdKey).toString();
        _folderUsers[userId] = {userId,
                                folderUser.value(usersCertificateKey).toString().toUtf8(),
                                folderUser.value(usersEncryptedMetadataKey).toString().toUtf8()};
    }

    if (_folderUsers.contains(_account->davUser())) {
        const auto currentFolderUser = _folderUsers.value(_account->davUser());
        const auto currentFolderUserEncryptedMetadataKey = currentFolderUser.encryptedMetadataKey;
        _metadataKey = QByteArray::fromBase64(decryptData(currentFolderUserEncryptedMetadataKey));
    }

    if (_metadataKey.isEmpty()) {
        qCDebug(lcCseMetadata()) << "Could not decrypt metadata key!";
        return;
    }

    _fileDrop = metaDataDoc.object().value("filedrop").toObject();

    const auto nonce = metadataObj["nonce"].toString().toLocal8Bit();
    const auto authenticationTag = metadataObj["authenticationTag"].toString().toLocal8Bit();
    const auto cipherText = metadataObj["ciphertext"].toString().toLocal8Bit();
    const auto cipherTextDecrypted =decryptJsonObject(cipherText, _metadataKey);
    const auto cipherTextDocument = QJsonDocument::fromJson(cipherTextDecrypted);
    const auto keyChecksums = cipherTextDocument[keyChecksumsKey].toArray();
    for (auto it = keyChecksums.constBegin(); it != keyChecksums.constEnd(); ++it) {
        const auto keyChecksum = it->toVariant().toString().toUtf8();
        if (!keyChecksum.isEmpty()) {
            _keyChecksums.insert(it->toVariant().toString().toUtf8());
        }
    }

    if (keyChecksums.isEmpty()) {
        if (_topLevelFolderMetadata) {
            _keyChecksums = _topLevelFolderMetadata->keyChecksums();
        }
    }

    if (!verifyMetadataKey()) {
        qCDebug(lcCseMetadata()) << "Could not verify metadataKey!";
        return;
    }

    if (cipherTextDecrypted.isEmpty()) {
        qCDebug(lcCseMetadata()) << "Could not decrypt metadata key!";
        return;
    }

    const auto sharing = cipherTextDocument["sharing"].toString().toLocal8Bit();
    const auto files = cipherTextDocument.object()["files"].toObject();
    const auto folders = cipherTextDocument.object()["folders"].toObject();

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

bool FolderMetadata::isMetadataSetup() const
{
    return !_metadataKey.isEmpty() || !_metadataKeys.isEmpty();
}

EncryptedFile FolderMetadata::parseFileAndFolderFromJson(const QString &encryptedFilename, const QJsonValue &fileJSON) const
{
    EncryptedFile file;
    file.encryptedFilename = encryptedFilename;

    auto fileObj = fileJSON.toObject();
    file.metadataKey = fileObj[metadataKeyKey].toInt();
    file.authenticationTag = QByteArray::fromBase64(fileObj["authenticationTag"].toString().toLocal8Bit());
    file.initializationVector = QByteArray::fromBase64(fileObj["initializationVector"].toString().toLocal8Bit());

    // Decrypt encrypted part
    auto encryptedFile = fileObj["encrypted"].toString().toLocal8Bit();
    auto decryptedFile = !_metadataKey.isEmpty() ? decryptJsonObject(encryptedFile, _metadataKey) : QByteArray{};
    auto decryptedFileDoc = QJsonDocument::fromJson(decryptedFile);

    auto decryptedFileObj = decryptedFileDoc.object();

    if (decryptedFileObj["filename"].toString().isEmpty()) {
        qCDebug(lcCseMetadata()) << "decrypted metadata" << decryptedFileDoc.toJson(QJsonDocument::Indented);
        qCWarning(lcCseMetadata()) << "skipping encrypted file" << file.encryptedFilename << "metadata has an empty file name";
        return {};
    }

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

void FolderMetadata::setupEmptyMetadata() {
    qCDebug(lcCseMetadata()) << "Setting up empty metadata";
    if (_topLevelFolderMetadata) {
        _metadataKey = _topLevelFolderMetadata->metadataKey();
        _keyChecksums = _topLevelFolderMetadata->keyChecksums();
    }

    if (!_topLevelFolderMetadata && _topLevelFolderPath.isEmpty() || _topLevelFolderPath == QStringLiteral("/")) {
        const auto encryptedLatestMetadataKey = encryptData(_metadataKey.toBase64());
        _folderUsers[_account->davUser()] = {_account->davUser(), _account->e2e()->_certificate.toPem(), encryptedLatestMetadataKey};
    }

    if (_metadataKey.isEmpty()) {
        createNewMetadataKey();
    }

    QString publicKey = _account->e2e()->_publicKey.toPem().toBase64();
    QString displayName = _account->displayName();

    _sharing.append({displayName, publicKey});
}

void FolderMetadata::encryptMetadata()
{
    qCDebug(lcCseMetadata()) << "Generating metadata";

    if ((_versionFromMetadata == -1 || _versionFromMetadata > 1) && !_topLevelFolderPath.isEmpty() && _topLevelFolderPath != QStringLiteral("/")) {
        _isEncryptionRequested = true;
        fetchTopLevelFolderEncryptedId();
        return;
    }
    if (_versionFromMetadata > 1) {
        handleEncryption();
    } else {
        handleEncryptionV1();
    }
}

void FolderMetadata::handleEncryption()
{
    _isEncryptionRequested = false;
    if (_metadataKey.isEmpty()) {
        qCDebug(lcCseMetadata()) << "Metadata generation failed! Empty metadata key!";
        QTimer::singleShot(0, this, [this]() {
            emit encryptionFinished({});
        });
        return;
    }

    QJsonArray folderUsers;
    for (auto it = _folderUsers.constBegin(), end = _folderUsers.constEnd(); it != end; ++it) {
        const auto folderUser = it.value();

        const QJsonObject folderUserJson = {{usersUserIdKey, folderUser.userId},
                                            {usersCertificateKey, QJsonValue::fromVariant(folderUser.certificatePem)},
                                            {usersEncryptedMetadataKey, QJsonValue::fromVariant(folderUser.encryptedMetadataKey)}};
        folderUsers.push_back(folderUserJson);
    }

    QJsonObject files;
    QJsonObject folders;
    for (auto it = _files.constBegin(), end = _files.constEnd(); it != end; it++) {
        QJsonObject encrypted;
        encrypted.insert("key", QString(it->encryptionKey.toBase64()));
        encrypted.insert("filename", it->originalFilename);
        encrypted.insert("mimetype", QString(it->mimetype));
        encrypted.insert(versionKey, it->fileVersion);
        QJsonDocument encryptedDoc;
        encryptedDoc.setObject(encrypted);

        const QString encryptedEncrypted = encryptJsonObject(encryptedDoc.toJson(QJsonDocument::Compact), _metadataKey);
        if (encryptedEncrypted.isEmpty()) {
            qCDebug(lcCseMetadata()) << "Metadata generation failed!";
        }

        QJsonObject file;
        file.insert("encrypted", encryptedEncrypted);
        file.insert("initializationVector", QString(it->initializationVector.toBase64()));
        file.insert("authenticationTag", QString(it->authenticationTag.toBase64()));

        if (it->mimetype == QByteArrayLiteral("httpd/unix-directory") || it->mimetype == QByteArrayLiteral("inode/directory")) {
            folders.insert(it->encryptedFilename, file);
        } else {
            files.insert(it->encryptedFilename, file);
        }
    }

    QJsonArray keyChecksums;
    for (auto it = _keyChecksums.constBegin(), end = _keyChecksums.constEnd(); it != end; ++it) {
        keyChecksums.push_back(QJsonValue::fromVariant(*it));
    }

    QJsonObject cipherText = {// {"sharing", sharingEncrypted},
                              {"files", files},
                              {"folders", folders}};

    if (!keyChecksums.isEmpty()) {
        cipherText.insert(keyChecksumsKey, keyChecksums);
    }

    QJsonDocument cipherTextDoc = QJsonDocument(cipherText);

    QJsonObject metadata = {
        {"ciphertext", QJsonValue::fromVariant(encryptJsonObject(cipherTextDoc.toJson(QJsonDocument::Compact), _metadataKey))},
        // {"sharing", sharingEncrypted},
        {"nonce", "123"},
        {"authenticationTag", "123"},
    };
    QJsonObject metaObject = {{metadataJsonKey, metadata}, {versionKey, 2}};

    if (!folderUsers.isEmpty()) {
        metaObject.insert(usersKey, folderUsers);
    }

    QJsonDocument internalMetadata;
    internalMetadata.setObject(metaObject);

    QTimer::singleShot(0, this, [this, internalMetadata]() {
        emit encryptionFinished(internalMetadata.toJson());
    });
}

void FolderMetadata::handleEncryptionV1()
{
    qCDebug(lcCseMetadata) << "Generating metadata";

    if (_metadataKey.isEmpty()) {
        qCDebug(lcCseMetadata) << "Metadata generation failed! Empty metadata key!";
        QTimer::singleShot(0, this, [this]() {
            emit encryptionFinished({});
        });
        return;
    }

    QJsonObject metadataKeys;
    for (auto it = _metadataKeys.constBegin(), end = _metadataKeys.constEnd(); it != end; it++) {
        /*
         * We have to already base64 encode the metadatakey here. This was a misunderstanding in the RFC
         * Now we should be compatible with Android and IOS. Maybe we can fix it later.
         */
        const QByteArray encryptedKey = encryptData(_metadataKey.toBase64());
        metadataKeys.insert(QString::number(it.key()), QString(encryptedKey));
    }

    QJsonObject metadata = {{"metadataKeys", metadataKeys},
                            // {"sharing", sharingEncrypted},
                            {"version", 1}};

    QJsonObject files;
    for (auto it = _files.constBegin(), end = _files.constEnd(); it != end; it++) {
        QJsonObject encrypted;
        encrypted.insert("key", QString(it->encryptionKey.toBase64()));
        encrypted.insert("filename", it->originalFilename);
        encrypted.insert("mimetype", QString(it->mimetype));
        encrypted.insert("version", it->fileVersion);
        QJsonDocument encryptedDoc;
        encryptedDoc.setObject(encrypted);

        QString encryptedEncrypted = encryptJsonObject(encryptedDoc.toJson(QJsonDocument::Compact), _metadataKeys.last());
        if (encryptedEncrypted.isEmpty()) {
            qCDebug(lcCseMetadata) << "Metadata generation failed!";
        }

        QJsonObject file;
        file.insert("encrypted", encryptedEncrypted);
        file.insert("initializationVector", QString(it->initializationVector.toBase64()));
        file.insert("authenticationTag", QString(it->authenticationTag.toBase64()));
        file.insert("metadataKey", _metadataKeys.lastKey());

        files.insert(it->encryptedFilename, file);
    }

    QJsonObject metaObject = {{"metadata", metadata}, {"files", files}};

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
    return _fileDrop.size() > 0;
}

bool FolderMetadata::moveFromFileDropToFiles()
{
    if (_fileDrop.isEmpty()) {
        return false;
    }

    for (auto it = _fileDrop.constBegin(); it != _fileDrop.constEnd(); ++it) {
        const auto fileObject = it.value().toObject();

        const auto encryptedFile = fileObject["encrypted"].toString().toLocal8Bit();
        const auto decryptedFile = decryptData(encryptedFile);
        const auto decryptedFileDocument = QJsonDocument::fromJson(decryptedFile);
        const auto decryptedFileObject = decryptedFileDocument.object();

        EncryptedFile file;
        file.encryptedFilename = it.key();
        file.metadataKey = fileObject[metadataKeyKey].toInt();
        file.authenticationTag = QByteArray::fromBase64(fileObject["authenticationTag"].toString().toLocal8Bit());
        file.initializationVector = QByteArray::fromBase64(fileObject["initializationVector"].toString().toLocal8Bit());

        file.originalFilename = decryptedFileObject["filename"].toString();
        file.encryptionKey = QByteArray::fromBase64(decryptedFileObject["key"].toString().toLocal8Bit());
        file.mimetype = decryptedFileObject["mimetype"].toString().toLocal8Bit();
        file.fileVersion = decryptedFileObject[versionKey].toInt();

        // In case we wrongly stored "inode/directory" we try to recover from it
        if (file.mimetype == QByteArrayLiteral("inode/directory")) {
            file.mimetype = QByteArrayLiteral("httpd/unix-directory");
        }

        _files.push_back(file);
    }

    return true;
}

const QJsonObject &FolderMetadata::fileDrop() const
{
    return _fileDrop;
}

void FolderMetadata::fetchTopLevelFolderEncryptedId()
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
    if (!job) {
        topLevelFolderEncryptedMetadataReceived({}, 404);
        return;
    }

    if (job->_folderInfos.isEmpty()) {
        topLevelFolderEncryptedMetadataReceived({}, 404);
        return;
    }

    const auto &folderInfo = job->_folderInfos.value(list.first());
    
    fetchTopLevelFolderMetadata(folderInfo.fileId);
}

void FolderMetadata::topLevelFolderEncryptedMetadataError(const QByteArray &fileId, int httpReturnCode)
{
    Q_UNUSED(fileId);
    Q_UNUSED(httpReturnCode);
    topLevelFolderEncryptedMetadataReceived({}, httpReturnCode);
}

void FolderMetadata::topLevelFolderEncryptedMetadataReceived(const QJsonDocument &json, int statusCode)
{
    _topLevelFolderMetadata.reset(new FolderMetadata(_account, json.toJson(QJsonDocument::Compact), statusCode));
    if (_topLevelFolderMetadata->versionFromMetadata() == -1 || _topLevelFolderMetadata->versionFromMetadata() > 1) {
        _metadataKey = _topLevelFolderMetadata->metadataKey();
        _keyChecksums = _topLevelFolderMetadata->keyChecksums();
    }
    if (_isEncryptionRequested) {
        if (_versionFromMetadata > 1) {
            handleEncryption();
        } else {
            handleEncryptionV1();
        }
    } else {
        setupMetadata();
    }
}

void FolderMetadata::topLevelFolderEncryptedIdError(QNetworkReply *r)
{
    topLevelFolderEncryptedMetadataReceived({}, r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
}

bool FolderMetadata::addUser(const QString &userId, const QSslCertificate certificate)
{
    if (_topLevelFolderMetadata) {
        return _topLevelFolderMetadata->addUser(userId, certificate);
    }
    Q_ASSERT(!userId.isEmpty() && !certificate.isNull());
    if (userId.isEmpty()) {
        qCDebug(lcCseMetadata()) << "Could not add a folder user. Invalid userId.";
        return false;
    }

    if (certificate.isNull()) {
        qCDebug(lcCseMetadata()) << "Could not add a folder user. Invalid certificate.";
        return false;
    }
    const auto certificatePublicKey = certificate.publicKey();
    if (certificatePublicKey.isNull()) {
        qCDebug(lcCseMetadata()) << "Could not add a folder user. Invalid certificate.";
        return false;
    }
    
    _folderUsers[userId] = {userId, certificate.toPem(), {}};

    createNewMetadataKey();
    updateUsersEncryptedMetadataKey();

    return true;
}

bool FolderMetadata::removeUser(const QString &userId)
{
    if (_topLevelFolderMetadata) {
        return _topLevelFolderMetadata->removeUser(userId);
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
    if (_topLevelFolderMetadata) {
        _topLevelFolderMetadata->updateUsersEncryptedMetadataKey();
        return;
    }
    Q_ASSERT(!_metadataKey.isEmpty());
    if (_metadataKey.isEmpty()) {
        qCWarning(lcCseMetadata()) << "Could not update folder users with empty metadataKey!";
        return;
    }
    for (auto it = _folderUsers.constBegin(); it != _folderUsers.constEnd(); ++it) {
        const auto folderUser = it.value();

        const QSslCertificate certificate(folderUser.certificatePem);
        if (certificate.isNull()) {
            qCWarning(lcCseMetadata()) << "Could not update folder users with null certificate!";
            continue;
        }

        const auto certificatePublicKey = certificate.publicKey();
        if (certificatePublicKey.isNull()) {
            qCWarning(lcCseMetadata()) << "Could not update folder users with null certificatePublicKey!";
            continue;
        }

        const auto encryptedMetadataKey = encryptData(_metadataKey.toBase64(), certificatePublicKey);
        if (encryptedMetadataKey.isEmpty()) {
            qCWarning(lcCseMetadata()) << "Could not update folder users with empty encryptedMetadataKey!";
            continue;
        }
        _folderUsers[it.key()] = {folderUser.userId, folderUser.certificatePem, encryptedMetadataKey};
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
    _metadataKeys.insert(0, _metadataKey);
}

bool FolderMetadata::verifyMetadataKey() const
{
    const auto md5MetadataKey = calcMd5(_metadataKey);
    return _keyChecksums.contains(md5MetadataKey);
}
}
