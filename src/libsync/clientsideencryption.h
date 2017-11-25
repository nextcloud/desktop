#ifndef CLIENTSIDEENCRYPTION_H
#define CLIENTSIDEENCRYPTION_H

#include <QString>
#include <QObject>
#include <QJsonDocument>

#include <openssl/rsa.h>
#include <openssl/evp.h>
#include <string>

#include "accountfwd.h"
#include "networkjobs.h"

#include <nlohmann/json.hpp>

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
    void getPrivateKeyFromServer();
    void getPublicKeyFromServer();
    void encryptPrivateKey(EVP_PKEY *keyPair);
		void setTokenForFolder(const QByteArray& folder, const QByteArray& token);
		QByteArray tokenForFolder(const QByteArray& folder) const;

		//TODO: Perhaps mode this to FolderStatusModel
		// (as it makes sense, but it increase the chance
		// of conflicts).
		void fetchFolderEncryptedStatus();

private slots:
		void folderEncryptedStatusFetched(const QMap<QString, bool> &values);
		void folderEncryptedStatusError(int error);

signals:
    void initializationFinished();

private:
    OCC::AccountPtr _account;
    bool isInitialized = false;
		bool _refreshingEncryptionStatus = false;
		//TODO: Save this on disk.
		QMap<QByteArray, QByteArray> _folder2token;
		QMap<QByteArray, bool> _folder2encryptedStatus;
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
    QByteArray encryptMetadataKeys(const nlohmann::json& metadataKeys) const;
    std::string decryptMetadataKeys(const std::string& encryptedMetadataKeys) const;

    std::string genMetadataPass() const;
    QByteArray encryptJsonObject(const nlohmann::json& obj, const QByteArray pass) const;
    std::string decryptJsonObject(const std::string& encryptedJsonBlob, const std::string& pass) const;

    QVector<EncryptedFile> _files;
    QVector<int> _metadataKeys;
    AccountPtr _account;
		QByteArray _metadata;
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
class OWNCLOUDSYNC_EXPORT GetFolderEncryptStatus : public AbstractNetworkJob
{
	Q_OBJECT
public:
	explicit GetFolderEncryptStatus (const AccountPtr &account, QObject *parent = 0);

public slots:
	void start() override;

protected:
	bool finished() override;

signals:
	void encryptStatusReceived(const QMap<QString, bool> folderMetadata2EncryptionStatus);
	void encryptStatusError(int statusCode);
};

} // namespace OCC
#endif
