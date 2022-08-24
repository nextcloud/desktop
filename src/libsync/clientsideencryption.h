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

#include <openssl/evp.h>

#include "accountfwd.h"
#include "networkjobs.h"

namespace QKeychain {
class Job;
class WritePasswordJob;
class ReadPasswordJob;
}

namespace OCC {

QString e2eeBaseUrl();

namespace EncryptionHelper {
    QByteArray generateRandomFilename();
    OWNCLOUDSYNC_EXPORT QByteArray generateRandom(int size);
    QByteArray generatePassword(const QString &wordlist, const QByteArray& salt);
    OWNCLOUDSYNC_EXPORT QByteArray encryptPrivateKey(
            const QByteArray& key,
            const QByteArray& privateKey,
            const QByteArray &salt
    );
    OWNCLOUDSYNC_EXPORT QByteArray decryptPrivateKey(
            const QByteArray& key,
            const QByteArray& data
    );
    OWNCLOUDSYNC_EXPORT QByteArray extractPrivateKeySalt(const QByteArray &data);
    OWNCLOUDSYNC_EXPORT QByteArray encryptStringSymmetric(
            const QByteArray& key,
            const QByteArray& data
    );
    OWNCLOUDSYNC_EXPORT QByteArray decryptStringSymmetric(
            const QByteArray& key,
            const QByteArray& data
    );

    QByteArray privateKeyToPem(const QByteArray key);

    //TODO: change those two EVP_PKEY into QSslKey.
    QByteArray encryptStringAsymmetric(
            EVP_PKEY *publicKey,
            const QByteArray& data
    );
    QByteArray decryptStringAsymmetric(
            EVP_PKEY *privateKey,
            const QByteArray& data
    );

    OWNCLOUDSYNC_EXPORT bool fileEncryption(const QByteArray &key, const QByteArray &iv,
                      QFile *input, QFile *output, QByteArray& returnTag);

    OWNCLOUDSYNC_EXPORT bool fileDecryption(const QByteArray &key, const QByteArray &iv,
                               QFile *input, QFile *output);

//
// Simple classes for safe (RAII) handling of OpenSSL
// data structures
//
class CipherCtx {
public:
    CipherCtx() : _ctx(EVP_CIPHER_CTX_new())
    {
    }

    ~CipherCtx()
    {
        EVP_CIPHER_CTX_free(_ctx);
    }

    operator EVP_CIPHER_CTX*()
    {
        return _ctx;
    }

private:
    Q_DISABLE_COPY(CipherCtx)
    EVP_CIPHER_CTX *_ctx;
};

class OWNCLOUDSYNC_EXPORT StreamingDecryptor
{
public:
    StreamingDecryptor(const QByteArray &key, const QByteArray &iv, quint64 totalSize);
    ~StreamingDecryptor() = default;

    QByteArray chunkDecryption(const char *input, quint64 chunkSize);

    bool isInitialized() const;
    bool isFinished() const;

private:
    Q_DISABLE_COPY(StreamingDecryptor)

    CipherCtx _ctx;
    bool _isInitialized = false;
    bool _isFinished = false;
    quint64 _decryptedSoFar = 0;
    quint64 _totalSize = 0;
};
}

class OWNCLOUDSYNC_EXPORT ClientSideEncryption : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString mnemonic READ mnemonic NOTIFY mnemonicChanged)
    Q_PROPERTY(bool serverHasPublicKey READ serverHasPublicKey NOTIFY serverHasPublicKeyChanged)
    Q_PROPERTY(bool serverHasPrivateKey READ serverHasPrivateKey NOTIFY serverHasPrivateKeyChanged)
    Q_PROPERTY(bool keychainHasPublicKey READ keychainHasPublicKey NOTIFY keychainHasPublicKeyChanged)
    Q_PROPERTY(bool keychainHasPrivateKey READ keychainHasPrivateKey NOTIFY keychainHasPrivateKeyChanged)
    Q_PROPERTY(QSslKey publicKey READ publicKey NOTIFY publicKeyChanged)
    Q_PROPERTY(QByteArray privateKey READ privateKey NOTIFY privateKeyChanged)
public:
    ClientSideEncryption();
    void initialize(const AccountPtr &account);

    const QString &mnemonic() const;

    const QSslKey &publicKey() const;
    const QByteArray &privateKey() const;
    bool serverHasPublicKey() const;
    bool serverHasPrivateKey() const;
    bool keychainHasPublicKey() const;
    bool keychainHasPrivateKey() const;

    void forgetSensitiveData(const AccountPtr &account);
    bool newMnemonicGenerated() const;

    void checkServerForKeys(const AccountPtr &account);
public slots:
    void slotRequestMnemonic();

    void setMnemonicGenerated(const QString &newMnemonic);

signals:
    void initializationFinished();
    void mnemonicChanged(const QString& mnemonic);
    void showMnemonic(const QString& mnemonic);
    void serverHasPublicKeyChanged();
    void serverHasPrivateKeyChanged();
    void keychainHasPublicKeyChanged();
    void keychainHasPrivateKeyChanged();
    void publicKeyChanged();
    void privateKeyChanged();

private slots:
    void publicKeyFetched(QKeychain::Job *incoming);
    void privateKeyFetched(QKeychain::Job *incoming);
    void mnemonicKeyFetched(QKeychain::Job *incoming);

    void setMnemonic(const QString &newMnemonic);
    void setServerHasPublicKey(bool newServerHasPublicKey);
    void setServerHasPrivateKey(bool newServerHasPrivateKey);
    void setKeychainHasPublicKey(bool newKeychainHasPublicKey);
    void setKeychainHasPrivateKey(bool newKeychainHasPrivateKey);

private:
    void generateKeyPair(const AccountPtr &account);
    void generateCSR(const AccountPtr &account, EVP_PKEY *keyPair);
    void encryptPrivateKey(const AccountPtr &account);
    void getPrivateKeyFromServer(const AccountPtr &account);
    void getPublicKeyFromServer(const AccountPtr &account);
    void fetchAndValidatePublicKeyFromServer(const AccountPtr &account);
    void decryptPrivateKey(const AccountPtr &account, const QByteArray &key);

    void fetchFromKeyChain(const AccountPtr &account);

    bool checkPublicKeyValidity(const AccountPtr &account) const;
    bool checkServerPublicKeyValidity(const QByteArray &serverPublicKeyString) const;
    void writePrivateKey(const AccountPtr &account);
    void writeCertificate(const AccountPtr &account);
    void writeMnemonic(const AccountPtr &account);

    bool isInitialized = false;

    QByteArray _privateKey;
    QSslKey _publicKey;
    QSslCertificate _certificate;
    QString _mnemonic;
    bool _newMnemonicGenerated = false;
    bool _serverHasPublicKey = false;
    bool _serverHasPrivateKey = false;
    bool _keychainHasPublicKey = false;
    bool _keychainHasPrivateKey = false;
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

class OWNCLOUDSYNC_EXPORT FolderMetadata {
public:
    FolderMetadata(AccountPtr account, const QByteArray& metadata = QByteArray(), int statusCode = -1);
    QByteArray encryptedMetadata();
    void addEncryptedFile(const EncryptedFile& f);
    void removeEncryptedFile(const EncryptedFile& f);
    void removeAllEncryptedFiles();
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
