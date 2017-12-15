#ifndef CLIENTSIDEENCRYPTION_H
#define CLIENTSIDEENCRYPTION_H

#include <QString>
#include <QObject>
#include <QJsonDocument>
#include <QSslCertificate>
#include <QSslKey>
#include <QFile>

#include <openssl/rsa.h>
#include <openssl/evp.h>
#include <string>

#include "accountfwd.h"
#include "networkjobs.h"

#include <nlohmann/json.hpp>

namespace QKeychain {
class Job;
class WritePasswordJob;
class ReadPasswordJob;
}

namespace OCC {

QString baseUrl();
QString baseDirectory();
QString privateKeyPath(AccountPtr account);
QString publicKeyPath(AccountPtr account);

class ClientSideEncryption : public QObject {
    Q_OBJECT
public:
    ClientSideEncryption();
    void initialize();
    void setAccount(AccountPtr account);
    bool hasPrivateKey() const;
    bool hasPublicKey() const;
    void generateKeyPair();
    void generateCSR(EVP_PKEY *keyPair);
    void encryptPrivateKey();
    void setTokenForFolder(const QByteArray& folder, const QByteArray& token);
    QByteArray tokenForFolder(const QByteArray& folder) const;
    void fetchFolderEncryptedStatus();

    // to be used together with FolderStatusModel::FolderInfo::_path.
    bool isFolderEncrypted(const QString& path);
    void setFolderEncryptedStatus(const QString& path, bool status);

private slots:
    void folderEncryptedStatusFetched(const QMap<QString, bool> &values);
    void folderEncryptedStatusError(int error);

    void publicKeyFetched(QKeychain::Job *incoming);
    void privateKeyFetched(QKeychain::Job *incoming);
    void mnemonicKeyFetched(QKeychain::Job *incoming);

signals:
    void initializationFinished();

private:
    void getPrivateKeyFromServer();
    void getPublicKeyFromServer();
    void decryptPrivateKey(const QByteArray &key);

    void fetchFromKeyChain();

    void writePrivateKey();
    void writeCertificate();
    void writeMnemonic();

    AccountPtr _account;
    bool isInitialized = false;
    bool _refreshingEncryptionStatus = false;
    //TODO: Save this on disk.
    QMap<QByteArray, QByteArray> _folder2token;
    QMap<QString, bool> _folder2encryptedStatus;

public:
    QSslKey _privateKey;
    QSslKey _publicKey;
    QSslCertificate _certificate;
    QString _mnemonic;
};

/* Generates the Metadata for the folder */
struct EncryptedFile {
    QByteArray encryptionKey;
    QByteArray mimetype;
    QByteArray initializationVector;
    QByteArray authenticationTag;
    QString encryptedFilename;
    QString originalFilename;
    int fileVersion;
    int metadataKey;
};

class FolderMetadata {
public:
    FolderMetadata(AccountPtr account, const QByteArray& metadata = QByteArray());
    QByteArray encryptedMetadata();
    void addEncryptedFile(const EncryptedFile& f);
    QVector<EncryptedFile> files() const;


private:
    /* Use std::string and std::vector internally on this class
     * to ease the port to Nlohmann Json API
     */
    void setupEmptyMetadata();
    void setupExistingMetadata();

    QByteArray encryptMetadataKeys(const nlohmann::json& metadataKeys) const;
    std::string decryptMetadataKeys(const QByteArray& encryptedMetadataKeysb64) const;

    std::string genMetadataPass() const;
    QByteArray encryptJsonObject(const nlohmann::json& obj, const QByteArray pass) const;
    std::string decryptJsonObject(const std::string& encryptedJsonBlob, const std::string& pass) const;

    QVector<EncryptedFile> _files;
    QVector<int> _metadataKeys;
    AccountPtr _account;
    QByteArray _metadata;
};

class FileEncryptionJob : public QObject
{
    Q_OBJECT
public:
    FileEncryptionJob(QByteArray &key, QByteArray &iv, QFile *input, QFile *output, QObject *parent = 0);

public slots:
    void start();

signals:
    void finished(QFile *output);

private:
    QByteArray _key;
    QByteArray _iv;
    QPointer<QFile> _input;
    QPointer<QFile> _output;
};

class FileDecryptionJob : public QObject
{
    Q_OBJECT
public:
    FileDecryptionJob(QByteArray &key, QByteArray &iv, QFile *input, QFile *output, QObject *parent = 0);

public slots:
    void start();

signals:
    void finished(QFile *output);

private:
    QByteArray _key;
    QByteArray _iv;
    QPointer<QFile> _input;
    QPointer<QFile> _output;
};


} // namespace OCC
#endif
