#include "clientsideencryptionjobs.h"
#include <QDebug>
#include <QLoggingCategory>
#include <QFileInfo>
#include <QDir>
#include <QJsonObject>
#include <QXmlStreamReader>
#include <QXmlStreamNamespaceDeclaration>
#include <QStack>
#include <QInputDialog>
#include <QLineEdit>

#include "clientsideencryption.h"
#include "account.h"
#include "capabilities.h"
#include "networkjobs.h"
#include "clientsideencryptionjobs.h"
#include "theme.h"
#include "creds/abstractcredentials.h"
#include "common/syncjournaldb.h"

Q_LOGGING_CATEGORY(lcSignPublicKeyApiJob, "nextcloud.sync.networkjob.sendcsr", QtInfoMsg)
Q_LOGGING_CATEGORY(lcStorePublicKeyApiJob, "nextcloud.sync.networkjob.storepublickey", QtInfoMsg)
Q_LOGGING_CATEGORY(lcStorePrivateKeyApiJob, "nextcloud.sync.networkjob.storeprivatekey", QtInfoMsg)
Q_LOGGING_CATEGORY(lcCseJob, "nextcloud.sync.networkjob.clientsideencrypt", QtInfoMsg)

namespace
{
constexpr auto e2eeSignatureHeaderName = "X-NC-E2EE-SIGNATURE";
}

namespace OCC {

GetMetadataApiJob::GetMetadataApiJob(const AccountPtr& account,
                                    const QByteArray& fileId,
                                    QObject* parent)
    : AbstractNetworkJob(account, e2eeBaseUrl(account) + QStringLiteral("meta-data/") + fileId, parent)
    , _fileId(fileId)
{
}

const QByteArray &GetMetadataApiJob::signature() const
{
    return _signature;
}

void GetMetadataApiJob::start()
{
    QNetworkRequest req;
    req.setRawHeader("OCS-APIREQUEST", "true");
    QUrlQuery query;
    query.addQueryItem(QLatin1String("format"), QLatin1String("json"));
    QUrl url = Utility::concatUrlPath(account()->url(), path());
    url.setQuery(query);

    qCInfo(lcCseJob()) << "Requesting the metadata for the fileId" << _fileId << "as encrypted";
    sendRequest("GET", url, req);
    AbstractNetworkJob::start();
}

bool GetMetadataApiJob::finished()
{
    int retCode = reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (retCode != 200) {
        qCInfo(lcCseJob()) << "error requesting the metadata" << path() << errorString() << retCode;
        emit error(_fileId, retCode);
        return true;
    }
    if (_account->capabilities().clientSideEncryptionVersion() >= 2.0) {
        _signature = reply()->rawHeader(e2eeSignatureHeaderName);
    }
    QJsonParseError error{};
    const auto replyData = reply()->readAll();
    auto json = QJsonDocument::fromJson(replyData, &error);
    qCInfo(lcCseJob) << "metadata received for file id" << _fileId << json.toJson(QJsonDocument::Compact);
    emit jsonReceived(json, reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
    return true;
}

StoreMetaDataApiJob::StoreMetaDataApiJob(const AccountPtr& account,
                                         const QByteArray& fileId,
                                         const QByteArray &token,
                                         const QByteArray& b64Metadata,
                                         const QByteArray &signature,
                                         QObject* parent)
    : AbstractNetworkJob(account, e2eeBaseUrl(account) + QStringLiteral("meta-data/") + fileId, parent),
    _fileId(fileId),
    _token(token),
    _b64Metadata(b64Metadata),
    _signature(signature)
{
}

void StoreMetaDataApiJob::start()
{
    QNetworkRequest req;
    req.setRawHeader("OCS-APIREQUEST", "true");
    req.setHeader(QNetworkRequest::ContentTypeHeader, QByteArrayLiteral("application/x-www-form-urlencoded"));
    if (_account->capabilities().clientSideEncryptionVersion() >= 2.0) {
        if (!_signature.isEmpty()) {
            req.setRawHeader(e2eeSignatureHeaderName, _signature);
        } else {
            qCWarning(lcCseJob()) << "empty signature for" << _fileId;
        }
    }
    QUrlQuery query;
    query.addQueryItem(QLatin1String("format"), QLatin1String("json"));
    if (_account->capabilities().clientSideEncryptionVersion() < 2.0) {
        query.addQueryItem(QStringLiteral("e2e-token"), _token);
    } else {
        req.setRawHeader(QByteArrayLiteral("e2e-token"), _token);
    }
    QUrl url = Utility::concatUrlPath(account()->url(), path());
    url.setQuery(query);

    QByteArray data = QByteArray("metaData=") + QUrl::toPercentEncoding(_b64Metadata);
    auto buffer = new QBuffer(this);
    buffer->setData(data);

    qCInfo(lcCseJob()) << "sending the metadata for the fileId" << _fileId << "as encrypted";
    sendRequest("POST", url, req, buffer);
    AbstractNetworkJob::start();
}

bool StoreMetaDataApiJob::finished()
{
    const auto retCode = reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (retCode != 200) {
        qCInfo(lcCseJob()) << "error sending the metadata" << path() << errorString() << retCode;
        emit error(_fileId, retCode);
        return false;
    }

    qCInfo(lcCseJob()) << "Metadata submitted to the server successfully";
    emit success(_fileId);

    return true;
}

UpdateMetadataApiJob::UpdateMetadataApiJob(const AccountPtr& account,
                                                 const QByteArray& fileId,
                                                 const QByteArray& b64Metadata,
                                                 const QByteArray& token,
                                                 const QByteArray& signature,
                                                 QObject* parent)
: AbstractNetworkJob(account, e2eeBaseUrl(account) + QStringLiteral("meta-data/") + fileId, parent)
, _fileId(fileId),
_b64Metadata(b64Metadata),
_token(token),
_signature(signature)
{
}

void UpdateMetadataApiJob::start()
{
    QNetworkRequest req;
    req.setRawHeader("OCS-APIREQUEST", "true");
    req.setHeader(QNetworkRequest::ContentTypeHeader, QByteArrayLiteral("application/x-www-form-urlencoded"));

    if (_account->capabilities().clientSideEncryptionVersion() >= 2.0) {
        if (!_signature.isEmpty()) {
            req.setRawHeader(e2eeSignatureHeaderName, _signature);
        }
    }

    QUrlQuery urlQuery;
    urlQuery.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));

    if (_account->capabilities().clientSideEncryptionVersion() < 2.0) {
        urlQuery.addQueryItem(QStringLiteral("e2e-token"), _token);
    } else {
        req.setRawHeader(QByteArrayLiteral("e2e-token"), _token);
    }

    QUrl url = Utility::concatUrlPath(account()->url(), path());
    url.setQuery(urlQuery);

    QUrlQuery params;
    params.addQueryItem("metaData",QUrl::toPercentEncoding(_b64Metadata));

    QByteArray data = params.query().toLocal8Bit();
    auto buffer = new QBuffer(this);
    buffer->setData(data);

    qCInfo(lcCseJob()) << "updating the metadata for the fileId" << _fileId << "as encrypted";
    sendRequest("PUT", url, req, buffer);
    AbstractNetworkJob::start();
}

bool UpdateMetadataApiJob::finished()
{
    int retCode = reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		if (retCode != 200) {
			qCInfo(lcCseJob()) << "error updating the metadata" << path() << errorString() << retCode;
			emit error(_fileId, retCode);
            return false;
		}

		qCInfo(lcCseJob()) << "Metadata submitted to the server successfully";
		emit success(_fileId);
    return true;
}

UnlockEncryptFolderApiJob::UnlockEncryptFolderApiJob(const AccountPtr& account,
                                                 const QByteArray& fileId,
                                                 const QByteArray& token,
                                                 SyncJournalDb *journalDb,
                                                 QObject* parent)
    : AbstractNetworkJob(account, e2eeBaseUrl(account) + QStringLiteral("lock/") + fileId, parent)
    , _fileId(fileId)
    , _token(token)
    , _journalDb(journalDb)
{
}

void UnlockEncryptFolderApiJob::start()
{
    QNetworkRequest req;
    req.setRawHeader("OCS-APIREQUEST", "true");
    req.setRawHeader("e2e-token", _token);

    QUrl url = Utility::concatUrlPath(account()->url(), path());

    if (shouldRollbackMetadataChanges()) {
        QUrlQuery query(url);
        query.addQueryItem(QLatin1String("abort"), QLatin1String("true"));
        url.setQuery(query);
    }

    sendRequest("DELETE", url, req);

    AbstractNetworkJob::start();
    qCInfo(lcCseJob()) << "Starting the request to unlock.";

    qCInfo(lcCseJob()) << "unlock folder started for:" << path() << " for fileId: " << _fileId;
}

void UnlockEncryptFolderApiJob::setShouldRollbackMetadataChanges(bool shouldRollbackMetadataChanges)
{
    _shouldRollbackMetadataChanges = shouldRollbackMetadataChanges;
}

[[nodiscard]] bool UnlockEncryptFolderApiJob::shouldRollbackMetadataChanges() const
{
    return _shouldRollbackMetadataChanges;
}

bool UnlockEncryptFolderApiJob::finished()
{
    int retCode = reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    qCInfo(lcCseJob()) << "unlock folder finished with code" << retCode << " for:" << path() << " for fileId: " << _fileId;

    if (retCode != 0) {
        _journalDb->deleteE2EeLockedFolder(_fileId);
    }

    emit done();

    if (retCode != 200) {
        qCInfo(lcCseJob()) << "error unlocking file" << path() << errorString() << retCode;
        qCInfo(lcCseJob()) << "Full Error Log" << reply()->readAll();
        emit error(_fileId, retCode, errorString());
        return true;
    }

    emit success(_fileId);
    return true;
}


DeleteMetadataApiJob::DeleteMetadataApiJob(const AccountPtr& account, const QByteArray& fileId, const QByteArray &token, QObject* parent)
: AbstractNetworkJob(account, e2eeBaseUrl(account) + QStringLiteral("meta-data/") + fileId, parent), 
_fileId(fileId),
_token(token)
{
}

void DeleteMetadataApiJob::start()
{
    QNetworkRequest req;
    req.setRawHeader("OCS-APIREQUEST", "true");
    req.setRawHeader(QByteArrayLiteral("e2e-token"), _token);

    QUrl url = Utility::concatUrlPath(account()->url(), path());
    sendRequest("DELETE", url, req);

    AbstractNetworkJob::start();
    qCInfo(lcCseJob()) << "Starting the request to remove the metadata.";
}

bool DeleteMetadataApiJob::finished()
{
    int retCode = reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (retCode != 200) {
        qCInfo(lcCseJob()) << "error removing metadata for" << path() << errorString() << retCode;
        qCInfo(lcCseJob()) << "Full Error Log" << reply()->readAll();
        emit error(_fileId, retCode);
        return true;
    }
    emit success(_fileId);
    return true;
}

LockEncryptFolderApiJob::LockEncryptFolderApiJob(const AccountPtr &account,
                                                 const QByteArray &fileId,
                                                 const QByteArray &certificateSha256Fingerprint,
                                                 SyncJournalDb *journalDb,
                                                 const QSslKey &sslkey,
                                                 QObject *parent)
    : AbstractNetworkJob(account, e2eeBaseUrl(account) + QStringLiteral("lock/") + fileId, parent)
    , _fileId(fileId)
    , _certificateSha256Fingerprint(certificateSha256Fingerprint)
    , _journalDb(journalDb)
{
    Q_UNUSED(sslkey)
}

void LockEncryptFolderApiJob::start()
{
    const auto folderTokenEncrypted = _journalDb->e2EeLockedFolder(_fileId);

    if (!folderTokenEncrypted.isEmpty()) {
        qCInfo(lcCseJob()) << "lock folder started for:" << path() << " for fileId: " << _fileId << " but we need to first lift the previous lock";
        const auto folderToken = EncryptionHelper::decryptStringAsymmetric(_account->e2e()->getCertificateInformation(), _account->e2e()->paddingMode(), *_account->e2e(), folderTokenEncrypted);
        if (!folderToken) {
            qCWarning(lcCseJob()) << "decrypt failed";
            return;
        }
        const auto unlockJob = new OCC::UnlockEncryptFolderApiJob(_account, _fileId, *folderToken, _journalDb, this);
        unlockJob->setShouldRollbackMetadataChanges(true);
        connect(unlockJob, &UnlockEncryptFolderApiJob::done, this, [this]() {
            this->start();
        });
        unlockJob->start();
        return;
    }

    QNetworkRequest req;
    req.setRawHeader("OCS-APIREQUEST", "true");
    if (_account->capabilities().clientSideEncryptionVersion() >= 2.0) {
        if (_counter > 0) {
            req.setRawHeader("X-NC-E2EE-COUNTER", QByteArray::number(_counter));
        }
    }
    QUrlQuery query;
    query.addQueryItem(QLatin1String("format"), QLatin1String("json"));
    QUrl url = Utility::concatUrlPath(account()->url(), path());
    url.setQuery(query);

    qCInfo(lcCseJob()) << "locking the folder with id" << _fileId << "as encrypted";

    sendRequest("POST", url, req);
    AbstractNetworkJob::start();

    qCInfo(lcCseJob()) << "lock folder started for:" << path() << " for fileId: " << _fileId;
}

bool LockEncryptFolderApiJob::finished()
{
    int retCode = reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (retCode != 200) {
        qCInfo(lcCseJob()) << "error locking file" << path() << errorString() << retCode;
        emit error(_fileId, retCode, errorString());
        qCInfo(lcCseJob()) << "lock folder finished with code" << retCode << " for:" << path() << " for fileId: " << _fileId;
        return true;
    }

    QJsonParseError error{};
    const auto json = QJsonDocument::fromJson(reply()->readAll(), &error);
    const auto obj = json.object().toVariantMap();
    const auto token = obj["ocs"].toMap()["data"].toMap()["e2e-token"].toByteArray();
    qCInfo(lcCseJob()) << "got json:" << token;

    qCInfo(lcCseJob()) << "lock folder finished with code" << retCode << " for:" << path() << " for fileId: " << _fileId << " token:" << token;

    if (!_account->e2e()->getPublicKey().isNull()) {
        const auto folderTokenEncrypted = EncryptionHelper::encryptStringAsymmetric(_account->e2e()->getCertificateInformation(), _account->e2e()->paddingMode(), *_account->e2e(), token);
        if (!folderTokenEncrypted) {
            qCWarning(lcCseJob()) << "decrypt failed";
            return false;
        }
        _journalDb->setE2EeLockedFolder(_fileId, *folderTokenEncrypted);
    }

    //TODO: Parse the token and submit.
    emit success(_fileId, token);
    return true;
}

void LockEncryptFolderApiJob::setCounter(quint64 counter)
{
    _counter = counter;
}

SetEncryptionFlagApiJob::SetEncryptionFlagApiJob(const AccountPtr& account, const QByteArray& fileId, FlagAction flagAction, QObject* parent)
: AbstractNetworkJob(account, e2eeBaseUrl(account) + QStringLiteral("encrypted/") + fileId, parent), _fileId(fileId), _flagAction(flagAction)
{
}

void SetEncryptionFlagApiJob::start()
{
    QNetworkRequest req;
    req.setRawHeader("OCS-APIREQUEST", "true");
    QUrl url = Utility::concatUrlPath(account()->url(), path());

    qCInfo(lcCseJob()) << "marking the file with id" << _fileId << "as" << (_flagAction == Set ? "encrypted" : "non-encrypted") << ".";

    sendRequest(_flagAction == Set ? "PUT" : "DELETE", url, req);

    AbstractNetworkJob::start();
}

bool SetEncryptionFlagApiJob::finished()
{
    int retCode = reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    qCInfo(lcCseJob()) << "Encryption Flag Return" << reply()->readAll();
    if (retCode == 200) {
        emit success(_fileId);
    } else {
        qCInfo(lcCseJob()) << "Setting the encrypted flag failed with" << path() << errorString() << retCode;
        emit error(_fileId, retCode, errorString());
    }
    return true;
}

StorePublicKeyApiJob::StorePublicKeyApiJob(const AccountPtr& account, const QString& path, QObject* parent)
    : AbstractNetworkJob(account, path, parent)
{
}

void StorePublicKeyApiJob::setPublicKey(const QByteArray& publicKey)
{
    QByteArray data = "publicKey=";
    data += QUrl::toPercentEncoding(publicKey);
    _publicKey.setData(data);
}

void StorePublicKeyApiJob::start()
{
    QNetworkRequest req;
    req.setRawHeader("OCS-APIREQUEST", "true");
    req.setHeader(QNetworkRequest::ContentTypeHeader, QByteArrayLiteral("application/x-www-form-urlencoded"));
    QUrlQuery query;
    query.addQueryItem(QLatin1String("format"), QLatin1String("json"));
    QUrl url = Utility::concatUrlPath(account()->url(), path());
    url.setQuery(query);

    qCDebug(lcStorePublicKeyApiJob) << "Sending the public key";
    sendRequest("PUT", url, req, &_publicKey);
    AbstractNetworkJob::start();
}

bool StorePublicKeyApiJob::finished()
{
    int retCode = reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (retCode != 200)
        qCInfo(lcStorePublicKeyApiJob()) << "Sending public key ended with"  << path() << errorString() << retCode;

    QJsonParseError error{};
    auto json = QJsonDocument::fromJson(reply()->readAll(), &error);
    emit jsonReceived(json, reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
    return true;
}

StorePrivateKeyApiJob::StorePrivateKeyApiJob(const AccountPtr& account, const QString& path, QObject* parent)
: AbstractNetworkJob(account, path, parent)
{
}

void StorePrivateKeyApiJob::setPrivateKey(const QByteArray& privKey)
{
    QByteArray data = "privateKey=";
    data += QUrl::toPercentEncoding(privKey);
    _privKey.setData(data);
}

void StorePrivateKeyApiJob::start()
{
    QNetworkRequest req;
    req.setRawHeader("OCS-APIREQUEST", "true");
    QUrlQuery query;
    query.addQueryItem(QLatin1String("format"), QLatin1String("json"));
    QUrl url = Utility::concatUrlPath(account()->url(), path());
    url.setQuery(query);

    qCDebug(lcStorePrivateKeyApiJob) << "Sending the private key";
    sendRequest("POST", url, req, &_privKey);
    AbstractNetworkJob::start();
}

bool StorePrivateKeyApiJob::finished()
{
    int retCode = reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (retCode != 200)
        qCInfo(lcStorePrivateKeyApiJob()) << "Sending private key ended with"  << path() << errorString() << retCode;

    QJsonParseError error{};
    auto json = QJsonDocument::fromJson(reply()->readAll(), &error);
    emit jsonReceived(json, reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
    return true;
}

SignPublicKeyApiJob::SignPublicKeyApiJob(const AccountPtr& account, const QString& path, QObject* parent)
: AbstractNetworkJob(account, path, parent)
{
}

void SignPublicKeyApiJob::setCsr(const QByteArray& csr)
{
    QByteArray data = "csr=";
    data += QUrl::toPercentEncoding(csr);
    _csr.setData(data);
}

void SignPublicKeyApiJob::start()
{
    QNetworkRequest req;
    req.setRawHeader("OCS-APIREQUEST", "true");
    req.setHeader(QNetworkRequest::ContentTypeHeader, QByteArrayLiteral("application/x-www-form-urlencoded"));
    QUrlQuery query;
    query.addQueryItem(QLatin1String("format"), QLatin1String("json"));
    QUrl url = Utility::concatUrlPath(account()->url(), path());
    url.setQuery(query);

    qCDebug(lcSignPublicKeyApiJob) << "Sending the CSR";
    sendRequest("POST", url, req, &_csr);
    AbstractNetworkJob::start();
}

bool SignPublicKeyApiJob::finished()
{
    qCInfo(lcStorePrivateKeyApiJob()) << "Sending CSR ended with"  << path() << errorString() << reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute);

    QJsonParseError error{};
    auto json = QJsonDocument::fromJson(reply()->readAll(), &error);
    emit jsonReceived(json, reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
    return true;
}

}
