#pragma once

#include "accountfwd.h"

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QSharedPointer>
#include <QSslCertificate>
#include <QSslKey>
#include <QString>
#include <QVector>

class QJsonDocument;
namespace OCC
{

class OWNCLOUDSYNC_EXPORT FolderMetadata : public QObject
{
    Q_OBJECT
    struct FolderUser {
        QString userId;
        QByteArray certificatePem;
        QByteArray encryptedMetadataKey;
        QByteArray encryptedFiledropKey;
    };

public:
    FolderMetadata(AccountPtr account,
                   const QByteArray &metadata,
                   const QString &topLevelFolderPath,
                   const QSharedPointer<FolderMetadata> &topLevelFolderMetadata = {},
                   QObject *parent = nullptr);
    [[nodiscard]] QVector<EncryptedFile> files() const;

    [[nodiscard]] bool isMetadataSetup() const;

    [[nodiscard]] bool isFileDropPresent() const;

    [[nodiscard]] bool moveFromFileDropToFiles();

    const QByteArray &fileDrop() const;

    bool addUser(const QString &userId, const QSslCertificate certificate);
    bool removeUser(const QString &userId);

    const QByteArray &metadataKey() const;
    const QSet<QByteArray> &keyChecksums() const;
    int versionFromMetadata() const;

    QByteArray encryptedMetadata();

private:
    /* Use std::string and std::vector internally on this class
     * to ease the port to Nlohmann Json API
     */
    [[nodiscard]] bool verifyMetadataKey(const QByteArray &metadataKey) const;

    [[nodiscard]] QByteArray encryptData(const QByteArray &data) const;
    [[nodiscard]] QByteArray encryptData(const QByteArray &data, const QSslKey key) const;
    [[nodiscard]] QByteArray decryptData(const QByteArray &data) const;

    [[nodiscard]] QByteArray encryptJsonObject(const QByteArray& obj, const QByteArray pass) const;
    [[nodiscard]] QByteArray decryptJsonObject(const QByteArray& encryptedJsonBlob, const QByteArray& pass) const;

    [[nodiscard]] QByteArray encryptCipherText(const QByteArray &cipherText, const QByteArray &pass, const QByteArray &initiaizationVector, QByteArray &returnTag) const;
    [[nodiscard]] QByteArray decryptCipherText(const QByteArray &encryptedCipherText, const QByteArray &pass, const QByteArray &initiaizationVector) const;

    [[nodiscard]] EncryptedFile parseFileAndFolderFromJson(const QString &encryptedFilename, const QJsonValue &fileJSON) const;

    [[nodiscard]] QJsonObject convertFileToJsonObject(const EncryptedFile *encryptedFile, const QByteArray &metadataKey) const;

    [[nodiscard]] bool isTopLevelFolder() const;
    
    QByteArray handleEncryptionRequestV2();
    QByteArray handleEncryptionRequestV1();

    static QByteArray gZipEncryptAndBase64Encode(const QByteArray &key, const QByteArray &inputData, const QByteArray &iv, QByteArray &returnTag);
    static QByteArray base64DecodeDecryptAndGzipUnZip(const QByteArray &key, const QByteArray &inputData, const QByteArray &iv);

public slots:
    void addEncryptedFile(const EncryptedFile &f);
    void removeEncryptedFile(const EncryptedFile &f);
    void removeAllEncryptedFiles();
    void setTopLevelFolderMetadata(const QSharedPointer<FolderMetadata> &topLevelFolderMetadata);

private slots:
    void setupMetadata();
    void setupEmptyMetadataV2();
    void setupEmptyMetadataV1();
    void setupExistingMetadata(const QByteArray &metadata);
    void setupExistingMetadataVersion1(const QByteArray &metadata);
    void setupExistingMetadataVersion2(const QByteArray &metadata);
    void startFetchTopLevelFolderMetadata();
    void fetchTopLevelFolderMetadata(const QByteArray &folderId);
    void topLevelFolderEncryptedIdReceived(const QStringList &list);
    void topLevelFolderEncryptedIdError(QNetworkReply *reply);
    void topLevelFolderEncryptedMetadataReceived(const QJsonDocument &json, int statusCode);
    void topLevelFolderEncryptedMetadataError(const QByteArray &fileId, int httpReturnCode);
    void updateUsersEncryptedMetadataKey();
    void createNewMetadataKey();
    void emitSetupComplete();

signals:
    void setupComplete();

private:
    QVector<EncryptedFile> _files;
    QByteArray _metadataKey;
    QByteArray _metadataNonce;
    QByteArray _fileDropMetadataNonce;
    QByteArray _fileDropMetadataAuthenticationTag;
    QByteArray _fileDropKey;
    QMap<int, QByteArray> _metadataKeys; //legacy, remove after migration is done
    QSet<QByteArray> _keyChecksums;
    QHash<QString, FolderUser> _folderUsers;
    AccountPtr _account;
    QVector<QPair<QString, QString>> _sharing;
    QByteArray _fileDropCipherTextEncryptedAndBase64;
    QByteArray _initialMetadata;
    QSharedPointer<FolderMetadata> _topLevelFolderMetadata;
    QString _topLevelFolderPath;
    int _versionFromMetadata = -1;
};

} // namespace OCC
