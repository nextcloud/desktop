#ifndef CLIENTSIDEENCRYPTION_H
#define CLIENTSIDEENCRYPTION_H

#include <QString>
#include <QObject>
#include <QJsonDocument>
#include <QSslCertificate>
#include <QSslKey>
#include <QFile>
#include <QVector>
#include <QMap>

#include <openssl/rsa.h>
#include <openssl/evp.h>

#include "accountfwd.h"
#include "networkjobs.h"

namespace QKeychain {
class Job;
class WritePasswordJob;
class ReadPasswordJob;
}

namespace OCC {

QString baseUrl();

class EncryptionHelper {
public:
    static QByteArray generateRandomString(int size);
    static QByteArray generateRandom(int size);
    static QByteArray generatePassword(const QString &wordlist, const QByteArray& salt);
    static QByteArray encryptPrivateKey(
            const QByteArray& key,
            const QByteArray& privateKey,
            const QByteArray &salt
    );
    static QByteArray decryptPrivateKey(
            const QByteArray& key,
            const QByteArray& data
    );
    static QByteArray encryptStringSymmetric(
            const QByteArray& key,
            const QByteArray& data
    );
    static QByteArray decryptStringSymmetric(
            const QByteArray& key,
            const QByteArray& data
    );

    //TODO: change those two EVP_PKEY into QSslKey.
    static QByteArray encryptStringAsymmetric(
            EVP_PKEY *publicKey,
            const QByteArray& data
    );
    static QByteArray decryptStringAsymmetric(
            EVP_PKEY *privateKey,
            const QByteArray& data
    );

    static QByteArray BIO2ByteArray(BIO *b);

    static bool fileEncryption(const QByteArray &key, const QByteArray &iv,
                      QFile *input, QFile *output, QByteArray& returnTag);

    static void fileDecryption(const QByteArray &key, const QByteArray& iv,
                               QFile *input, QFile *output);
};

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
    bool isFolderEncrypted(const QString& path) const;
    void setFolderEncryptedStatus(const QString& path, bool status);

    void forgetSensitiveData();

private slots:
    void folderEncryptedStatusFetched(const QMap<QString, bool> &values);
    void folderEncryptedStatusError(int error);

    void publicKeyFetched(QKeychain::Job *incoming);
    void privateKeyFetched(QKeychain::Job *incoming);
    void mnemonicKeyFetched(QKeychain::Job *incoming);

signals:
    void initializationFinished();
    void mnemonicGenerated(const QString& mnemonic);

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
    void setupExistingMetadata(const QByteArray& metadata);

    QByteArray encryptMetadataKey(const QByteArray& metadataKey) const;
    QByteArray decryptMetadataKey(const QByteArray& encryptedKey) const;

    QByteArray encryptJsonObject(const QByteArray& obj, const QByteArray pass) const;
    QByteArray decryptJsonObject(const QByteArray& encryptedJsonBlob, const QByteArray& pass) const;

    QVector<EncryptedFile> _files;
    QMap<int, QByteArray> _metadataKeys;
    AccountPtr _account;
    QVector<QPair<QString, QString>> _sharing;
};

} // namespace OCC
#endif
