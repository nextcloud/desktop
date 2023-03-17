#pragma once

#include <QString>
#include <QObject>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSslCertificate>
#include <QSslKey>
#include <QFile>
#include <QVector>
#include <QMap>

#include <openssl/evp.h>

#include "accountfwd.h"
#include "networkjobs.h"
#include "common/syncjournaldb.h"

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
    OWNCLOUDSYNC_EXPORT QByteArray encryptStringAsymmetric(const QSslKey key, const QByteArray &data);
    OWNCLOUDSYNC_EXPORT QByteArray decryptStringAsymmetric(const QByteArray &privateKeyPem, const QByteArray &data);

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

    OWNCLOUDSYNC_EXPORT bool dataEncryption(const QByteArray &key, const QByteArray &iv, const QByteArray &input, QByteArray &output, QByteArray &returnTag);
    OWNCLOUDSYNC_EXPORT bool dataDecryption(const QByteArray &key, const QByteArray &iv, const QByteArray &input, QByteArray &output);

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

    [[nodiscard]] bool isInitialized() const;
    [[nodiscard]] bool isFinished() const;

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
public:
    class PKey;

    ClientSideEncryption();

    QByteArray _privateKey;
    QSslKey _publicKey;
    QSslCertificate _certificate;
    QString _mnemonic;
    bool _newMnemonicGenerated = false;

    class Bio
    {
    public:
        Bio()
            : _bio(BIO_new(BIO_s_mem()))
        {
        }

        ~Bio()
        {
            BIO_free_all(_bio);
        }

        operator BIO *()
        {
            return _bio;
        }

    private:
        Q_DISABLE_COPY(Bio)

        BIO *_bio;
    };

    class PKeyCtx
    {
    public:
        explicit PKeyCtx(int id, ENGINE *e = nullptr)
            : _ctx(EVP_PKEY_CTX_new_id(id, e))
        {
        }

        ~PKeyCtx()
        {
            EVP_PKEY_CTX_free(_ctx);
        }

        // The move constructor is needed for pre-C++17 where
        // return-value optimization (RVO) is not obligatory
        // and we have a `forKey` static function that returns
        // an instance of this class
        PKeyCtx(PKeyCtx &&other)
        {
            std::swap(_ctx, other._ctx);
        }

        PKeyCtx &operator=(PKeyCtx &&other) = delete;

        static PKeyCtx forKey(EVP_PKEY *pkey, ENGINE *e = nullptr)
        {
            PKeyCtx ctx;
            ctx._ctx = EVP_PKEY_CTX_new(pkey, e);
            return ctx;
        }

        operator EVP_PKEY_CTX *()
        {
            return _ctx;
        }

    private:
        Q_DISABLE_COPY(PKeyCtx)

        PKeyCtx() = default;

        EVP_PKEY_CTX *_ctx = nullptr;
    };

    class PKey
    {
    public:
        ~PKey();

        // The move constructor is needed for pre-C++17 where
        // return-value optimization (RVO) is not obligatory
        // and we have a static functions that return
        // an instance of this class
        PKey(PKey &&other);

        PKey &operator=(PKey &&other) = delete;

        static PKey readPublicKey(Bio &bio);

        static PKey readPrivateKey(Bio &bio);

        static PKey generate(PKeyCtx &ctx);

        operator EVP_PKEY *()
        {
            return _pkey;
        }

        operator EVP_PKEY *() const
        {
            return _pkey;
        }

    private:
        Q_DISABLE_COPY(PKey)

        PKey() = default;

        EVP_PKEY *_pkey = nullptr;
    };

signals:
    void initializationFinished(bool isNewMnemonicGenerated = false);
    void sensitiveDataForgotten();
    void privateKeyDeleted();
    void certificateDeleted();
    void mnemonicDeleted();
    void certificatesFetchedFromServer(const QHash<QString, QSslCertificate> &results);
    void certificateFetchedFromKeychain(QSslCertificate certificate);
    void certificateFetchedFromServer(QSslCertificate certificate);
    void certificateWriteComplete(QSslCertificate certificate);

public:
    [[nodiscard]] QByteArray generateSignatureCMS(const QByteArray &data) const;
    [[nodiscard]] bool verifySignatureCMS(const QByteArray &cmsContent, const QByteArray &data) const;

public slots:
    void initialize(const AccountPtr &account);
    void forgetSensitiveData(const AccountPtr &account);
    void getUsersPublicKeyFromServer(const AccountPtr &account, const QStringList &userIds);
    void writeCertificate(const AccountPtr &account, const QString &userId, const QSslCertificate certificate);
    void fetchFromKeyChain(const AccountPtr &account, const QString &userId);

private slots:
    void generateKeyPair(const AccountPtr &account);
    void encryptPrivateKey(const AccountPtr &account);    

    void publicKeyFetched(QKeychain::Job *incoming);
    void publicKeyFetchedForUserId(QKeychain::Job *incoming);
    void privateKeyFetched(QKeychain::Job *incoming);
    void mnemonicKeyFetched(QKeychain::Job *incoming);

    void handlePrivateKeyDeleted(const QKeychain::Job* const incoming);
    void handleCertificateDeleted(const QKeychain::Job* const incoming);
    void handleMnemonicDeleted(const QKeychain::Job* const incoming);
    void checkAllSensitiveDataDeleted();

    void getPrivateKeyFromServer(const AccountPtr &account);
    void getPublicKeyFromServer(const AccountPtr &account);
    void fetchAndValidatePublicKeyFromServer(const AccountPtr &account);
    void decryptPrivateKey(const AccountPtr &account, const QByteArray &key);

    void fetchFromKeyChain(const AccountPtr &account);
    void writePrivateKey(const AccountPtr &account);
    void writeCertificate(const AccountPtr &account);
    void writeMnemonic(const AccountPtr &account);

private:
    void generateCSR(const AccountPtr &account, PKey keyPair);
    void sendSignRequestCSR(const AccountPtr &account, PKey keyPair, const QByteArray &csrContent);

    [[nodiscard]] bool checkPublicKeyValidity(const AccountPtr &account) const;
    [[nodiscard]] bool checkServerPublicKeyValidity(const QByteArray &serverPublicKeyString) const;
    [[nodiscard]] bool sensitiveDataRemaining() const;

    bool isInitialized = false;
};

/* Generates the Metadata for the folder */
struct EncryptedFile {
    QByteArray encryptionKey;
    QByteArray mimetype;
    QByteArray initializationVector;
    QByteArray authenticationTag;
    QString encryptedFilename;
    QString originalFilename;
    int fileVersion = 0;
    int metadataKey = 0;
};
} // namespace OCC