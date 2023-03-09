#pragma once

#include "accountfwd.h"
#include "common/syncjournaldb.h"

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
    };

public:
    FolderMetadata(AccountPtr account,
                   const QByteArray &metadata = {},
                   int statusCode = -1,
                   const QSharedPointer<FolderMetadata> &topLevelFolderMetadata = {},
                   const QString &topLevelFolderPath = {},
                   SyncJournalDb *journal = nullptr,
                   QObject *parent = nullptr);

    [[nodiscard]] QByteArray encryptedMetadata() const;
    [[nodiscard]] QVector<EncryptedFile> files() const;
    [[nodiscard]] bool isMetadataSetup() const;

    [[nodiscard]] bool isFileDropPresent() const;

    [[nodiscard]] bool moveFromFileDropToFiles();

    const QJsonObject &fileDrop() const;

    bool addUser(const QString &userId, const QSslCertificate certificate);
    bool removeUser(const QString &userId);

    const QByteArray &metadataKey() const;
    const QSet<QByteArray> &keyChecksums() const;

private:
    /* Use std::string and std::vector internally on this class
     * to ease the port to Nlohmann Json API
     */
    [[nodiscard]] bool verifyMetadataKey() const;

    [[nodiscard]] QByteArray encryptData(const QByteArray &data) const;
    [[nodiscard]] QByteArray encryptData(const QByteArray &data, const QSslKey key) const;
    [[nodiscard]] QByteArray decryptData(const QByteArray &data) const;

    [[nodiscard]] QByteArray encryptJsonObject(const QByteArray& obj, const QByteArray pass) const;
    [[nodiscard]] QByteArray decryptJsonObject(const QByteArray& encryptedJsonBlob, const QByteArray& pass) const;

    [[nodiscard]] EncryptedFile parseFileAndFolderFromJson(const QString &encryptedFilename, const QJsonValue &fileJSON) const;

public slots:
    void addEncryptedFile(const EncryptedFile &f);
    void removeEncryptedFile(const EncryptedFile &f);
    void removeAllEncryptedFiles();
    void setTopLevelFolderMetadata(const QSharedPointer<FolderMetadata> &topLevelFolderMetadata);
    bool unlockTopLevelFolder();

private slots:
    void setupMetadata();
    void setupEmptyMetadata();
    void setupExistingMetadata(const QByteArray &metadata);
    void setupExistingMetadataVersion1(const QByteArray &metadata);
    void setupExistingMetadataVersion2(const QByteArray &metadata);
    void fetchTopLevelFolderEncryptedId();
    void fetchTopLevelFolderMetadata();
    void topLevelFolderEncryptedIdReceived(const QStringList &list);
    void topLevelFolderEncryptedIdError(QNetworkReply *r);
    void topLevelFolderEncryptedMetadataReceived(const QJsonDocument &json, int statusCode);
    void topLevelFolderEncryptedMetadataError(const QByteArray &fileId, int httpReturnCode);
    void topLevelFolderLockedSuccessfully(const QByteArray &fileId, const QByteArray &token);
    void topLevelFolderLockedError(const QByteArray &fileId, int httpErrorCode);
    void lockTopLevelFolder();
    void updateUsersEncryptedMetadataKey();
    void createNewMetadataKey();
    void emitSetupComplete();

signals:
    void setupComplete();
    void topLevelFolderUnlocked();

private:
    QVector<EncryptedFile> _files;
    QByteArray _metadataKey;
    QSet<QByteArray> _keyChecksums;
    QHash<QString, FolderUser> _folderUsers;
    AccountPtr _account;
    QVector<QPair<QString, QString>> _sharing;
    QJsonObject _fileDrop;
    QByteArray _initialMetadata;
    int _initialStatusCode = -1;
    QSharedPointer<FolderMetadata> _topLevelFolderMetadata;
    QString _topLevelFolderPath;
    QPointer<SyncJournalDb> _journal = nullptr;
    QByteArray _topLevelFolderToken;
    QByteArray _topLevelFolderId;
    bool _isTopLevelFolderUnlockRunning = false;
};

} // namespace OCC
