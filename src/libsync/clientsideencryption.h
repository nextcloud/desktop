/*
 * Copyright © 2017, Tomaz Canabrava <tcanabrava@kde.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#ifndef CLIENTSIDEENCRYPTION_H
#define CLIENTSIDEENCRYPTION_H

#include "accountfwd.h"

#include "networkjobs.h"
#include "clientsideencryptiontokenselector.h"

#include <QString>
#include <QObject>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSslCertificate>
#include <QSslKey>
#include <QFile>
#include <QVector>
#include <QMap>

#include <libp11.h>

#include <openssl/evp.h>

#include <optional>

class QWidget;

namespace QKeychain {
class Job;
class WritePasswordJob;
class ReadPasswordJob;
}

namespace OCC {

class ClientSideEncryption;
QString e2eeBaseUrl();

class CertificateInformation {
public:
    CertificateInformation();

    CertificateInformation(PKCS11_KEY *publicKey,
                           PKCS11_KEY *privateKey,
                           QSslCertificate &&certificate);

    [[nodiscard]] bool operator==(const CertificateInformation &other) const;

    void clear();

    [[nodiscard]] QList<QSslError> verify() const;

    [[nodiscard]] bool isSelfSigned() const;

    [[nodiscard]] PKCS11_KEY* getPublicKey() const;

    [[nodiscard]] PKCS11_KEY* getPrivateKey() const;

    [[nodiscard]] bool canEncrypt() const;

    [[nodiscard]] bool canDecrypt() const;

    [[nodiscard]] bool userCertificateNeedsMigration() const;

    [[nodiscard]] bool sensitiveDataRemaining() const;

    [[nodiscard]] QByteArray sha256Fingerprint() const;

private:
    void checkEncryptionCertificate();

    PKCS11_KEY* _publicKey = nullptr;

    PKCS11_KEY* _privateKey = nullptr;

    QSslCertificate _certificate;

    bool _certificateExpired = true;

    bool _certificateNotYetValid = true;

    bool _certificateRevoked = true;

    bool _certificateInvalid = true;
};

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

[[nodiscard]] OWNCLOUDSYNC_EXPORT std::optional<QByteArray> encryptStringAsymmetric(const CertificateInformation &selectedCertificate,
                                                                                    const ClientSideEncryption &encryptionEngine,
                                                                                    const QByteArray &binaryData);

[[nodiscard]] OWNCLOUDSYNC_EXPORT std::optional<QByteArray> decryptStringAsymmetric(const CertificateInformation &selectedCertificate,
                                                                                    const ClientSideEncryption &encryptionEngine,
                                                                                    const QByteArray &base64Data,
                                                                                    const QByteArray &expectedCertificateSha256Fingerprint);

QByteArray privateKeyToPem(const QByteArray key);

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

    Q_PROPERTY(bool canEncrypt READ canEncrypt NOTIFY canEncryptChanged FINAL)
    Q_PROPERTY(bool canDecrypt READ canDecrypt NOTIFY canDecryptChanged FINAL)
    Q_PROPERTY(bool userCertificateNeedsMigration READ userCertificateNeedsMigration NOTIFY userCertificateNeedsMigrationChanged FINAL)
public:
    class PKey;

    ClientSideEncryption();

    [[nodiscard]] bool isInitialized() const;

    [[nodiscard]] bool tokenIsSetup() const;

    [[nodiscard]] const QSslKey& getPublicKey() const;

    void setPublicKey(const QSslKey &publicKey);

    [[nodiscard]] const QByteArray& getPrivateKey() const;

    void setPrivateKey(const QByteArray &privateKey);

    [[nodiscard]] const CertificateInformation& getTokenCertificate() const;

    [[nodiscard]] CertificateInformation getTokenCertificateByFingerprint(const QByteArray &expectedFingerprint) const;

    [[nodiscard]] bool useTokenBasedEncryption() const;

    [[nodiscard]] const QString &getMnemonic() const;

    void setCertificate(const QSslCertificate &certificate);

    [[nodiscard]] ENGINE* sslEngine() const;

    [[nodiscard]] ClientSideEncryptionTokenSelector* usbTokenInformation();

    [[nodiscard]] bool canEncrypt() const;

    [[nodiscard]] bool canDecrypt() const;

    [[nodiscard]] bool userCertificateNeedsMigration() const;

    [[nodiscard]] QByteArray certificateSha256Fingerprint() const;

signals:
    void initializationFinished(bool isNewMnemonicGenerated = false);
    void sensitiveDataForgotten();
    void privateKeyDeleted();
    void certificateDeleted();
    void mnemonicDeleted();
    void publicKeyDeleted();

    void startingDiscoveryEncryptionUsbToken();
    void finishedDiscoveryEncryptionUsbToken();

    void canEncryptChanged();
    void canDecryptChanged();
    void userCertificateNeedsMigrationChanged();

public slots:
    void initialize(QWidget *settingsDialog,
                    const OCC::AccountPtr &account);
    void initializeHardwareTokenEncryption(QWidget* settingsDialog,
                                           const OCC::AccountPtr &account);
    void forgetSensitiveData(const OCC::AccountPtr &account);

    void migrateCertificate();

private slots:
    void generateKeyPair(const OCC::AccountPtr &account);
    void encryptPrivateKey(const OCC::AccountPtr &account);

    void publicCertificateFetched(QKeychain::Job *incoming);
    void publicKeyFetched(QKeychain::Job *incoming);
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

    void completeHardwareTokenInitialization(QWidget *settingsDialog,
                                             const OCC::AccountPtr &account);

    void setMnemonic(const QString &mnemonic);

    void setEncryptionCertificate(CertificateInformation certificateInfo);

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

    [[nodiscard]] bool checkEncryptionIsWorking() const;

    void failedToInitialize(const AccountPtr &account);

    void saveCertificateIdentification(const AccountPtr &account) const;
    void cacheTokenPin(const QString pin);

    QByteArray _privateKey;
    QSslKey _publicKey;
    QSslCertificate _certificate;
    QString _mnemonic;
    bool _newMnemonicGenerated = false;

    QString _cachedPin;

    ClientSideEncryptionTokenSelector _usbTokenInformation;

    CertificateInformation _encryptionCertificate;
    std::vector<CertificateInformation> _otherCertificates;
};

/* Generates the Metadata for the folder */
struct EncryptedFile {
    QByteArray encryptionKey;
    QByteArray mimetype;
    QByteArray initializationVector;
    QByteArray authenticationTag;
    QString encryptedFilename;
    QString originalFilename;
};

class OWNCLOUDSYNC_EXPORT FolderMetadata {
public:
    enum class RequiredMetadataVersion {
        Version1,
        Version1_2,
    };

    explicit FolderMetadata(AccountPtr account);

    explicit FolderMetadata(AccountPtr account,
                            RequiredMetadataVersion requiredMetadataVersion,
                            const QByteArray& metadata,
                            int statusCode = -1);

    [[nodiscard]] QByteArray encryptedMetadata() const;
    void addEncryptedFile(const EncryptedFile& f);
    void removeEncryptedFile(const EncryptedFile& f);
    void removeAllEncryptedFiles();
    [[nodiscard]] QVector<EncryptedFile> files() const;
    [[nodiscard]] bool isMetadataSetup() const;

    [[nodiscard]] bool isFileDropPresent() const;

    [[nodiscard]] bool encryptedMetadataNeedUpdate(const QByteArray &expectedCertificateFingerprint) const;

    [[nodiscard]] bool moveFromFileDropToFiles();

    [[nodiscard]] QJsonObject fileDrop() const;

    [[nodiscard]] QByteArray certificateSha256Fingerprint() const;

private:
    /* Use std::string and std::vector internally on this class
     * to ease the port to Nlohmann Json API
     */
    void setupEmptyMetadata();
    void setupExistingMetadata(const QByteArray& metadata);

    [[nodiscard]] std::optional<QByteArray> encryptData(const QByteArray &binaryDatadata) const;
    [[nodiscard]] std::optional<QByteArray> decryptData(const QByteArray &base64Data) const;
    [[nodiscard]] QByteArray decryptDataUsingKey(const QByteArray &data,
                                                 const QByteArray &key,
                                                 const QByteArray &authenticationTag,
                                                 const QByteArray &initializationVector) const;

    [[nodiscard]] QByteArray encryptJsonObject(const QByteArray& obj, const QByteArray pass) const;
    [[nodiscard]] QByteArray decryptJsonObject(const QByteArray& encryptedJsonBlob, const QByteArray& pass) const;

    [[nodiscard]] bool checkMetadataKeyChecksum(const QByteArray &metadataKey, const QByteArray &metadataKeyChecksum) const;

    [[nodiscard]] QByteArray computeMetadataKeyChecksum(const QByteArray &metadataKey) const;

    QByteArray _metadataKey;

    QVector<EncryptedFile> _files;
    AccountPtr _account;
    RequiredMetadataVersion _requiredMetadataVersion = RequiredMetadataVersion::Version1_2;
    QVector<QPair<QString, QString>> _sharing;
    QJsonObject _fileDrop;
    // used by unit tests, must get assigned simultaneously with _fileDrop and not erased
    QJsonObject _fileDropFromServer;
    QByteArray _metadataCertificateSha256Fingerprint;
    bool _isMetadataSetup = false;
    bool _encryptedMetadataNeedUpdate = false;
};

} // namespace OCC
#endif
