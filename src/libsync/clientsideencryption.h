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

#include "owncloudlib.h"

#include "clientsideencryptionprimitives.h"
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
#include <QHash>

#include <libp11.h>

#include <openssl/evp.h>

#include <functional>
#include <optional>

class QWidget;

namespace QKeychain {
class Job;
class WritePasswordJob;
class ReadPasswordJob;
}

namespace OCC {

QString e2eeBaseUrl(const OCC::AccountPtr &account);

class ClientSideEncryption;

class CertificateInformation {
public:
    enum class CertificateType {
        SoftwareNextcloudCertificate,
        HardwareCertificate,
    };

    CertificateInformation();

    explicit CertificateInformation(PKCS11_KEY *hardwarePrivateKey,
                                    QSslCertificate &&certificate);

    explicit CertificateInformation(CertificateType certificateType,
                                    const QByteArray& privateKey,
                                    QSslCertificate &&certificate);

    [[nodiscard]] bool operator==(const CertificateInformation &other) const;

    void clear();

    [[nodiscard]] const QByteArray& getPrivateKeyData() const;

    void setPrivateKeyData(const QByteArray& privateKey);

    [[nodiscard]] QList<QSslError> verify() const;

    [[nodiscard]] bool isSelfSigned() const;

    [[nodiscard]] QSslKey getSslPublicKey() const;

    [[nodiscard]] PKey getEvpPublicKey() const;

    [[nodiscard]] PKCS11_KEY* getPkcs11PrivateKey() const;

    [[nodiscard]] PKey getEvpPrivateKey() const;

    [[nodiscard]] const QSslCertificate& getCertificate() const;

    [[nodiscard]] bool canEncrypt() const;

    [[nodiscard]] bool canDecrypt() const;

    [[nodiscard]] bool userCertificateNeedsMigration() const;

    [[nodiscard]] bool sensitiveDataRemaining() const;

    [[nodiscard]] QByteArray sha256Fingerprint() const;

private:
    void checkEncryptionCertificate();

    void doNotCheckEncryptionCertificate();

    PKCS11_KEY* _hardwarePrivateKey = nullptr;

    QByteArray _privateKeyData;

    QSslCertificate _certificate;

    CertificateType _certificateType = CertificateType::SoftwareNextcloudCertificate;

    bool _certificateExpired = true;

    bool _certificateNotYetValid = true;

    bool _certificateRevoked = true;

    bool _certificateInvalid = true;
};

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

[[nodiscard]] OWNCLOUDSYNC_EXPORT std::optional<QByteArray> encryptStringAsymmetric(const CertificateInformation &selectedCertificate,
                                                                                    int paddingMode,
                                                                                    const ClientSideEncryption &encryptionEngine,
                                                                                    const QByteArray &binaryData);

[[nodiscard]] OWNCLOUDSYNC_EXPORT std::optional<QByteArray> decryptStringAsymmetric(const CertificateInformation &selectedCertificate,
                                                                                    int paddingMode,
                                                                                    const ClientSideEncryption &encryptionEngine,
                                                                                    const QByteArray &base64Data);

QByteArray privateKeyToPem(const QByteArray key);

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

class OWNCLOUDSYNC_EXPORT NextcloudSslCertificate
{
public:
    NextcloudSslCertificate();
    NextcloudSslCertificate(const NextcloudSslCertificate &other);
    NextcloudSslCertificate(const QSslCertificate &certificate);
    NextcloudSslCertificate(QSslCertificate &&certificate);

    operator QSslCertificate();
    operator QSslCertificate() const;

    QSslCertificate& get();
    const QSslCertificate &get() const;

    NextcloudSslCertificate &operator=(const NextcloudSslCertificate &other);
    NextcloudSslCertificate &operator=(NextcloudSslCertificate &&other);

private:
    QSslCertificate _certificate;
};

class OWNCLOUDSYNC_EXPORT ClientSideEncryption : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool canEncrypt READ canEncrypt NOTIFY canEncryptChanged FINAL)
    Q_PROPERTY(bool canDecrypt READ canDecrypt NOTIFY canDecryptChanged FINAL)
    Q_PROPERTY(bool userCertificateNeedsMigration READ userCertificateNeedsMigration NOTIFY userCertificateNeedsMigrationChanged FINAL)
public:
    ClientSideEncryption();

    [[nodiscard]] bool isInitialized() const;

    [[nodiscard]] bool tokenIsSetup() const;

    [[nodiscard]] QSslKey getPublicKey() const;

    [[nodiscard]] const QByteArray& getPrivateKey() const;

    void setPrivateKey(const QByteArray &privateKey);

    [[nodiscard]] const CertificateInformation& getCertificateInformation() const;

    [[nodiscard]] CertificateInformation getCertificateInformationByFingerprint(const QByteArray &certificateFingerprint) const;

    [[nodiscard]] int paddingMode() const;

    [[nodiscard]] CertificateInformation getTokenCertificateByFingerprint(const QByteArray &expectedFingerprint) const;

    [[nodiscard]] bool useTokenBasedEncryption() const;

    [[nodiscard]] const QString &getMnemonic() const;

    void setCertificate(const QSslCertificate &certificate);

    [[nodiscard]] const QSslCertificate& getCertificate() const;

    [[nodiscard]] ENGINE* sslEngine() const;

    [[nodiscard]] QByteArray generateSignatureCryptographicMessageSyntax(const QByteArray &data) const;

    [[nodiscard]] bool verifySignatureCryptographicMessageSyntax(const QByteArray &cmsContent, const QByteArray &data, const QVector<QByteArray> &certificatePems) const;

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
    void certificateFetchedFromKeychain(QSslCertificate certificate);
    void certificatesFetchedFromServer(const QHash<QString, OCC::NextcloudSslCertificate> &results);
    void certificateWriteComplete(const QSslCertificate &certificate);

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
    void addExtraRootCertificates();
    void forgetSensitiveData(const OCC::AccountPtr &account);
    void getUsersPublicKeyFromServer(const OCC::AccountPtr &account, const QStringList &userIds);
    void fetchCertificateFromKeyChain(const OCC::AccountPtr &account, const QString &userId);
    void writeCertificate(const OCC::AccountPtr &account, const QString &userId, const QSslCertificate &certificate);

    void migrateCertificate();

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

    void completeHardwareTokenInitialization(QWidget *settingsDialog,
                                             const OCC::AccountPtr &account);

    void setMnemonic(const QString &mnemonic);

private:
    void generateMnemonic();

    void setEncryptionCertificate(CertificateInformation certificateInfo);

    [[nodiscard]] std::pair<QByteArray, PKey> generateCSR(const AccountPtr &account,
                                                          PKey keyPair,
                                                          PKey privateKey);

    void sendSignRequestCSR(const AccountPtr &account,
                            PKey keyPair,
                            const QByteArray &csrContent);

    void sendPublicKey(const AccountPtr &account);

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

    [[nodiscard]] bool checkServerPublicKeyValidity(const QByteArray &serverPublicKeyString) const;
    [[nodiscard]] bool sensitiveDataRemaining() const;

    [[nodiscard]] bool checkEncryptionIsWorking() const;

    void failedToInitialize(const AccountPtr &account);

    void saveCertificateIdentification(const AccountPtr &account) const;
    void cacheTokenPin(const QString pin);

    QString _mnemonic;
    bool _newMnemonicGenerated = false;

    QString _cachedPin;

    ClientSideEncryptionTokenSelector _usbTokenInformation;

    CertificateInformation _encryptionCertificate;
    std::vector<CertificateInformation> _otherCertificates;

    Pkcs11Context _context{Pkcs11Context::State::EmptyContext};
    std::unique_ptr<PKCS11_SLOT[], std::function<void(PKCS11_SLOT*)>> _tokenSlots;
};

} // namespace OCC

#endif
