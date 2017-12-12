#ifndef CLIENTSIDEENCRYPTIONJOBS_H
#define CLIENTSIDEENCRYPTIONJOBS_H

#include "networkjobs.h"
#include "accountfwd.h"
#include <QString>
#include <QJsonDocument>

namespace OCC {
/* Here are all of the network jobs for the client side encryption.
 * anything that goes thru the server and expects a response, is here.
 */

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
    explicit SetEncryptionFlagApiJob(const AccountPtr &account, const QByteArray& fileId, QObject *parent = 0);

public slots:
    void start() override;

protected:
    bool finished() override;

signals:
    void success(const QByteArray fileId);
    void error(const QByteArray fileId, int httpReturnCode);

private:
    QByteArray _fileId;
};

class OWNCLOUDSYNC_EXPORT LockEncryptFolderApiJob : public AbstractNetworkJob
{
    Q_OBJECT
public:
    explicit LockEncryptFolderApiJob(const AccountPtr &account, const QByteArray& fileId, QObject *parent = 0);

public slots:
    void start() override;

protected:
    bool finished() override;

signals:
    void success(const QByteArray& fileId, const QByteArray& token);
    void error(const QByteArray& fileId, int httpdErrorCode);

private:
    QByteArray _fileId;
};


class OWNCLOUDSYNC_EXPORT UnlockEncryptFolderApiJob : public AbstractNetworkJob
{
    Q_OBJECT
public:
    explicit UnlockEncryptFolderApiJob (
        const AccountPtr &account,
        const QByteArray& fileId,
        const QByteArray& token,
        QObject *parent = 0);

public slots:
    void start() override;

protected:
    bool finished() override;

signals:
    void success(const QByteArray& fileId);
    void error(const QByteArray& fileId, int httpReturnCode);

private:
    QByteArray _fileId;
    QByteArray _token;
    QBuffer *_tokenBuf;
};


class OWNCLOUDSYNC_EXPORT StoreMetaDataApiJob : public AbstractNetworkJob
{
    Q_OBJECT
public:
    explicit StoreMetaDataApiJob (
        const AccountPtr &account,
        const QByteArray& fileId,
        const QByteArray& b64Metadata,
        QObject *parent = 0);

public slots:
    void start() override;

protected:
    bool finished() override;

signals:
    void success(const QByteArray& fileId);
    void error(const QByteArray& fileId, int httpReturnCode);

private:
    QByteArray _fileId;
    QByteArray _b64Metadata;
};

class OWNCLOUDSYNC_EXPORT GetMetadataApiJob : public AbstractNetworkJob
{
    Q_OBJECT
public:
    explicit GetMetadataApiJob (
        const AccountPtr &account,
        const QByteArray& fileId,
        QObject *parent = 0);

public slots:
    void start() override;

protected:
    bool finished() override;

signals:
    void jsonReceived(const QJsonDocument &json, int statusCode);

private:
    QByteArray _fileId;
};

/* I cant use the propfind network job because it defaults to the
 * wrong dav url.
 */
class OWNCLOUDSYNC_EXPORT GetFolderEncryptStatusJob : public AbstractNetworkJob
{
	Q_OBJECT
public:
	explicit GetFolderEncryptStatusJob (const AccountPtr &account, const QString& folder, QObject *parent = 0);

public slots:
	void start() override;

protected:
	bool finished() override;

signals:
	void encryptStatusReceived(const QMap<QString, bool> folderMetadata2EncryptionStatus);
	void encryptStatusError(int statusCode);
private:
  QString _folder;
};

}
#endif
