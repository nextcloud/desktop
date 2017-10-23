#ifndef CLIENTSIDEENCRYPTION_H
#define CLIENTSIDEENCRYPTION_H

#include <QString>
#include <QObject>
#include <QJsonDocument>

#include <openssl/rsa.h>
#include <openssl/evp.h>

#include "accountfwd.h"
#include "networkjobs.h"

namespace OCC {

QString baseUrl();
QString baseDirectory();

class ClientSideEncryption : public QObject {
    Q_OBJECT
public:
    ClientSideEncryption();
    void initialize();
    void setAccount(AccountPtr account);
    bool hasPrivateKey() const;
    bool hasPublicKey() const;
    void generateKeyPair();
    QString generateCSR(EVP_PKEY *keyPair);
    void getPrivateKeyFromServer();
    void getPublicKeyFromServer();
    void encryptPrivateKey(EVP_PKEY *keyPair);
    QString privateKeyPath() const;
    QString publicKeyPath() const;

signals:
    void initializationFinished();

private:
    OCC::AccountPtr _account;
    bool isInitialized = false;
};


/*
 * @brief Job to sigh the CSR that return JSON
 *
 * To be used like this:
 * \code
 * _job = new SignPublicKeyApiJob(account, QLatin1String("ocs/v1.php/foo/bar"), this);
 * _job->setCsr( csr );
 * connect(_job...);
 * _job->start();
 * \encode
 *
 * @ingroup libsync
 */
class OWNCLOUDSYNC_EXPORT SignPublicKeyApiJob : public AbstractNetworkJob
{
    Q_OBJECT
public:
    explicit SignPublicKeyApiJob(const AccountPtr &account, const QString &path, QObject *parent = 0);

    /**
     * @brief setCsr - the CSR with the public key.
     * This function needs to be called before start() obviously.
     */
    void setCsr(const QByteArray& csr);

public slots:
    void start() override;

protected:
    bool finished() override;
signals:

    /**
     * @brief jsonReceived - signal to report the json answer from ocs
     * @param json - the parsed json document
     * @param statusCode - the OCS status code: 100 (!) for success
     */
    void jsonReceived(const QJsonDocument &json, int statusCode);

private:
    QBuffer _csr;
};

/*
 * @brief Job to upload the PrivateKey that return JSON
 *
 * To be used like this:
 * \code
 * _job = new StorePrivateKeyApiJob(account, QLatin1String("ocs/v1.php/foo/bar"), this);
 * _job->setPrivateKey( privKey );
 * connect(_job...);
 * _job->start();
 * \encode
 *
 * @ingroup libsync
 */
class OWNCLOUDSYNC_EXPORT StorePrivateKeyApiJob : public AbstractNetworkJob
{
    Q_OBJECT
public:
    explicit StorePrivateKeyApiJob(const AccountPtr &account, const QString &path, QObject *parent = 0);

    /**
     * @brief setCsr - the CSR with the public key.
     * This function needs to be called before start() obviously.
     */
    void setPrivateKey(const QByteArray& privateKey);

public slots:
    void start() override;

protected:
    bool finished() override;
signals:

    /**
     * @brief jsonReceived - signal to report the json answer from ocs
     * @param json - the parsed json document
     * @param statusCode - the OCS status code: 100 (!) for success
     */
    void jsonReceived(const QJsonDocument &json, int statusCode);

private:
    QBuffer _privKey;
};

/*
 * @brief Job to mark a folder as encrypted JSON
 *
 * To be used like this:
 * \code
 * _job = new SetEncryptionFlagApiJob(account, 2, this);
  * connect(_job...);
 * _job->start();
 * \encode
 *
 * @ingroup libsync
 */
class OWNCLOUDSYNC_EXPORT SetEncryptionFlagApiJob : public AbstractNetworkJob
{
    Q_OBJECT
public:
    explicit SetEncryptionFlagApiJob(const AccountPtr &account, const QString& fileId, QObject *parent = 0);

public slots:
    void start() override;

protected:
    bool finished() override;

signals:

    /**
     * @brief jsonReceived - signal to report the json answer from ocs
     * @param json - the parsed json document
     * @param statusCode - the OCS status code: 200 for success
     */
    void jsonReceived(const QJsonDocument &json, int statusCode);
private:
    QString _fileId;
};

} // namespace OCC

#endif
