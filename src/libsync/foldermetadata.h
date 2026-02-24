#pragma once
/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "accountfwd.h"
#include "encryptedfoldermetadatahandler.h"
#include "csync.h"
#include "rootencryptedfolderinfo.h"
#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QSslKey>
#include <QString>
#include <QVector>

class QSslCertificate;
class QJsonDocument;
class TestClientSideEncryptionV2;
class TestSecureFileDrop;
namespace OCC
{
 // Handles parsing and altering the metadata, encryption and decryption. Setup of the instance is always asynchronouse and emits void setupComplete()
class OWNCLOUDSYNC_EXPORT FolderMetadata : public QObject
{
    friend class ::TestClientSideEncryptionV2;
    friend class ::TestSecureFileDrop;
    Q_OBJECT

    struct UserWithFolderAccess {
        QString userId;
        QByteArray certificatePem;
        QByteArray encryptedMetadataKey;
    };

    // based on api-version and "version" key in metadata JSON
    enum MetadataVersion {
        VersionUndefined = -1,
        Version1,
        Version1_2,
        Version2_0,
    };

    struct UserWithFileDropEntryAccess {
        QString userId;
        QByteArray decryptedFiledropKey;

        inline bool isValid() const
        {
            return !userId.isEmpty() && !decryptedFiledropKey.isEmpty();
        }
    };

    struct FileDropEntry {
        QString encryptedFilename;
        QByteArray cipherText;
        QByteArray nonce;
        QByteArray authenticationTag;
        UserWithFileDropEntryAccess currentUser;
        
        inline bool isValid() const
        {
            return !cipherText.isEmpty() && !nonce.isEmpty() && !authenticationTag.isEmpty();
        }
    };

public:
    struct EncryptedFile {
        QByteArray encryptionKey;
        QByteArray mimetype;
        QByteArray initializationVector;
        QByteArray authenticationTag;
        QString encryptedFilename;
        QString originalFilename;
        bool isDirectory() const;
    };

    enum class FolderType {
        Nested = 0,
        Root = 1,
    };
    Q_ENUM(FolderType)

    enum class CertificateType {
        SoftwareNextcloudCertificate,
        HardwareCertificate,
    };
    Q_ENUM(CertificateType)

    FolderMetadata(AccountPtr account, const QString &remoteFolderRoot, FolderType folderType = FolderType::Nested);
    /*
    * construct metadata based on RootEncryptedFolderInfo
    * as per E2EE V2, the encryption key and users that have access are only stored in root(top-level) encrypted folder's metadata
    * see: https://github.com/nextcloud/end_to_end_encryption_rfc/blob/v2.1/RFC.md
    */
    FolderMetadata(AccountPtr account,
                   const QString &remoteFolderRoot,
                   const QByteArray &metadata,
                   const RootEncryptedFolderInfo &rootEncryptedFolderInfo,
                   const QByteArray &signature,
                   QObject *parent = nullptr);

    [[nodiscard]] QVector<EncryptedFile> files() const;

    [[nodiscard]] bool isValid() const;

    [[nodiscard]] bool isFileDropPresent() const;

    [[nodiscard]] bool isRootEncryptedFolder() const;

    [[nodiscard]] bool encryptedMetadataNeedUpdate() const;

    [[nodiscard]] bool moveFromFileDropToFiles();

    // adds a user to have access to this folder (always generates new metadata key)
    [[nodiscard]] bool addUser(const QString &userId, const QSslCertificate &certificate, CertificateType certificateType);
    // removes a user from this folder and removes and generates a new metadata key
    [[nodiscard]] bool removeUser(const QString &userId);

    [[nodiscard]] const QByteArray metadataKeyForEncryption() const;
    [[nodiscard]] const QByteArray metadataKeyForDecryption() const;
    [[nodiscard]] const QSet<QByteArray> &keyChecksums() const;

    [[nodiscard]] QByteArray encryptedMetadata();

    [[nodiscard]] EncryptionStatusEnums::ItemEncryptionStatus existingMetadataEncryptionStatus() const;
    [[nodiscard]] EncryptionStatusEnums::ItemEncryptionStatus encryptedMetadataEncryptionStatus() const;

    [[nodiscard]] bool isVersion2AndUp() const;

    [[nodiscard]] quint64 newCounter() const;

    [[nodiscard]] QByteArray metadataSignature() const;

    [[nodiscard]] QByteArray initialMetadata() const;

public slots:
    void addEncryptedFile(const OCC::FolderMetadata::EncryptedFile &f);
    void removeEncryptedFile(const OCC::FolderMetadata::EncryptedFile &f);
    void removeAllEncryptedFiles();

private:
    [[nodiscard]] QByteArray encryptedMetadataLegacy();

    [[nodiscard]] bool verifyMetadataKey(const QByteArray &metadataKey) const;

    [[nodiscard]] QByteArray encryptDataWithPublicKey(const QByteArray &data,
                                                      const CertificateInformation &shareUserCertificate) const;
    [[nodiscard]] QByteArray decryptDataWithPrivateKey(const QByteArray &data,
                                                       const QByteArray &base64CertificateSha256Hash) const;

    [[nodiscard]] QByteArray encryptJsonObject(const QByteArray& obj, const QByteArray pass) const;
    [[nodiscard]] QByteArray decryptJsonObject(const QByteArray& encryptedJsonBlob, const QByteArray& pass) const;

    [[nodiscard]] bool checkMetadataKeyChecksum(const QByteArray &metadataKey, const QByteArray &metadataKeyChecksum) const;

    [[nodiscard]] QByteArray computeMetadataKeyChecksum(const QByteArray &metadataKey) const;

    [[nodiscard]] EncryptedFile parseEncryptedFileFromJson(const QString &encryptedFilename, const QJsonValue &fileJSON) const;

    [[nodiscard]] QJsonObject convertFileToJsonObject(const EncryptedFile *encryptedFile) const;

    [[nodiscard]] MetadataVersion latestSupportedMetadataVersion() const;

    [[nodiscard]] bool parseFileDropPart(const QJsonDocument &doc);

    void setFileDrop(const QJsonObject &fileDrop);

    static EncryptionStatusEnums::ItemEncryptionStatus fromMedataVersionToItemEncryptionStatus(const MetadataVersion metadataVersion);
    static MetadataVersion fromItemEncryptionStatusToMedataVersion(const EncryptionStatusEnums::ItemEncryptionStatus encryptionStatus);

    static QByteArray prepareMetadataForSignature(const QJsonDocument &fullMetadata);

private slots:
    void initMetadata();
    void initEmptyMetadata();
    void initEmptyMetadataLegacy();

    void setupExistingMetadata(const QByteArray &metadata);
    void setupExistingMetadataLegacy(const QByteArray &metadata);

    void setupVersionFromExistingMetadata(const QByteArray &metadata);

    void startFetchRootE2eeFolderMetadata(const QString &path);
    void slotRootE2eeFolderMetadataReceived(int statusCode, const QString &message);

    void updateUsersEncryptedMetadataKey();
    void createNewMetadataKeyForEncryption();

    void emitSetupComplete();

signals:
    void setupComplete();

private:
    AccountPtr _account;
    QString _remoteFolderRoot;
    QByteArray _initialMetadata;

    bool _isRootEncryptedFolder = false;
    // always contains the last generated metadata key (non-encrypted and non-base64)
    QByteArray _metadataKeyForEncryption;
    // used for storing initial metadataKey to use for decryption, especially in nested folders when changing the metadataKey and re-encrypting nested dirs
    QByteArray _metadataKeyForDecryption;
    QByteArray _metadataNonce;
    // metadatakey checksums for validation during setting up from existing metadata
    QSet<QByteArray> _keyChecksums;

    // filedrop part non-parsed, for upload in case parsing can not be done (due to not having access for the current user, etc.)
    QJsonObject _fileDrop;
    // used by unit tests, must get assigned simultaneously with _fileDrop and never erased
    QJsonObject _fileDropFromServer;

    // legacy, remove after migration is done
    QMap<int, QByteArray> _metadataKeys;

    // users that have access to current folder's "ciphertext", except "filedrop" part
    QHash<QString, UserWithFolderAccess> _folderUsers;

    // must increment on each metadata upload
    quint64 _counter = 0;

    MetadataVersion _existingMetadataVersion = MetadataVersion::VersionUndefined;
    MetadataVersion _encryptedMetadataVersion = MetadataVersion::VersionUndefined;

    // generated each time QByteArray encryptedMetadata() is called, and will later be used for validation if uploaded
    QByteArray _metadataSignature;
    // signature from server-side metadata
    QByteArray _initialSignature;

    // both files and folders info
    QVector<EncryptedFile> _files;

    // parsed filedrop entries ready for move
    QVector<FileDropEntry> _fileDropEntries;

    // sets to "true" on successful parse
    bool _isMetadataValid = false;

    QScopedPointer<EncryptedFolderMetadataHandler> _encryptedFolderMetadataHandler;
};

} // namespace OCC
