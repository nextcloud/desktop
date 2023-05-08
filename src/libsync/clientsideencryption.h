#ifndef CLIENTSIDEENCRYPTION_H
#define CLIENTSIDEENCRYPTION_H

#include "clientsideencryptionprimitives.h"

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

namespace QKeychain {
class Job;
class WritePasswordJob;
class ReadPasswordJob;
}

namespace OCC {

QString e2eeBaseUrl(const OCC::AccountPtr &account);

namespace EncryptionHelper {
    OWNCLOUDSYNC_EXPORT QByteArray generateRandomFilename();
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

    OWNCLOUDSYNC_EXPORT QByteArray gzipThenEncryptData(const QByteArray &key, const QByteArray &inputData, const QByteArray &iv, QByteArray &returnTag);
    OWNCLOUDSYNC_EXPORT QByteArray decryptThenUnGzipData(const QByteArray &key, const QByteArray &inputData, const QByteArray &iv);

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
    ClientSideEncryption();

    QByteArray _privateKey;
    QSslKey _publicKey;
    QSslCertificate _certificate;
    QString _mnemonic;
    bool _newMnemonicGenerated = false;

signals:
    void initializationFinished(bool isNewMnemonicGenerated = false);
    void sensitiveDataForgotten();
    void privateKeyDeleted();
    void certificateDeleted();
    void mnemonicDeleted();
    void publicKeyDeleted();
    void certificateFetchedFromKeychain(QSslCertificate certificate);
    void certificatesFetchedFromServer(const QHash<QString, QSslCertificate> &results);
    void certificateWriteComplete(const QSslCertificate &certificate);

public:
    [[nodiscard]] QByteArray generateSignatureCryptographicMessageSyntax(const QByteArray &data) const;
    [[nodiscard]] bool verifySignatureCryptographicMessageSyntax(const QByteArray &cmsContent, const QByteArray &data, const QVector<QByteArray> &certificatePems) const;

public slots:
    void initialize(const OCC::AccountPtr &account);
    void forgetSensitiveData(const OCC::AccountPtr &account);
    void getUsersPublicKeyFromServer(const AccountPtr &account, const QStringList &userIds);
    void fetchCertificateFromKeyChain(const OCC::AccountPtr &account, const QString &userId);
    void writeCertificate(const AccountPtr &account, const QString &userId, const QSslCertificate &certificate);

private slots:
    void generateKeyPair(const OCC::AccountPtr &account);
    void encryptPrivateKey(const OCC::AccountPtr &account);

    void publicCertificateFetched(QKeychain::Job *incoming);
    void publicKeyFetched(QKeychain::Job *incoming);
    void publicKeyFetchedForUserId(QKeychain::Job *incoming);
    void privateKeyFetched(QKeychain::Job *incoming);
    void mnemonicKeyFetched(QKeychain::Job *incoming);

    void handlePrivateKeyDeleted(const QKeychain::Job* const incoming);
    void handleCertificateDeleted(const QKeychain::Job* const incoming);
    void handleMnemonicDeleted(const QKeychain::Job* const incoming);
    void handlePublicKeyDeleted(const QKeychain::Job* const incoming);
    void checkAllSensitiveDataDeleted();

    void getPrivateKeyFromServer(const OCC::AccountPtr &account);
    void getPublicKeyFromServer(const OCC::AccountPtr &account);
    void fetchAndValidatePublicKeyFromServer(const OCC::AccountPtr &account);
    void decryptPrivateKey(const OCC::AccountPtr &account, const QByteArray &key);

    void fetchCertificateFromKeyChain(const OCC::AccountPtr &account);
    void fetchPublicKeyFromKeyChain(const OCC::AccountPtr &account);
    void writePrivateKey(const OCC::AccountPtr &account);
    void writeCertificate(const OCC::AccountPtr &account);

private:
    void generateMnemonic();

    [[nodiscard]] std::pair<QByteArray, PKey> generateCSR(const AccountPtr &account,
                                                          PKey keyPair,
                                                          PKey privateKey);

    void sendSignRequestCSR(const AccountPtr &account,
                            PKey keyPair,
                            const QByteArray &csrContent);

    void writeKeyPair(const AccountPtr &account,
                      PKey keyPair,
                      const QByteArray &csrContent);

    template <typename L>
    void writeMnemonic(OCC::AccountPtr account,
                       L nextCall);

    void checkServerHasSavedKeys(const AccountPtr &account);

    template <typename SUCCESS_CALLBACK, typename ERROR_CALLBACK>
    void checkUserPublicKeyOnServer(const OCC::AccountPtr &account,
                                    SUCCESS_CALLBACK nextCheck,
                                    ERROR_CALLBACK onError);

    template <typename SUCCESS_CALLBACK, typename ERROR_CALLBACK>
    void checkUserPrivateKeyOnServer(const OCC::AccountPtr &account,
                                     SUCCESS_CALLBACK nextCheck,
                                     ERROR_CALLBACK onError);

    template <typename SUCCESS_CALLBACK, typename ERROR_CALLBACK>
    void checkUserKeyOnServer(const QString &keyType,
                              const OCC::AccountPtr &account,
                              SUCCESS_CALLBACK nextCheck,
                              ERROR_CALLBACK onError);

    [[nodiscard]] bool checkPublicKeyValidity(const AccountPtr &account) const;
    [[nodiscard]] bool checkServerPublicKeyValidity(const QByteArray &serverPublicKeyString) const;
    [[nodiscard]] bool sensitiveDataRemaining() const;

    void failedToInitialize(const AccountPtr &account);

    bool isInitialized = false;
};
} // namespace OCC
#endif
