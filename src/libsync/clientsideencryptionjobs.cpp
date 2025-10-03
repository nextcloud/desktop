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

Q_LOGGING_CATEGORY(lcSignPublicKeyApiJob, "nextcloud.sync.networkjob.sendcsr", QtInfoMsg)
Q_LOGGING_CATEGORY(lcStorePrivateKeyApiJob, "nextcloud.sync.networkjob.storeprivatekey", QtInfoMsg)
Q_LOGGING_CATEGORY(lcCseJob, "nextcloud.sync.networkjob.clientsideencrypt", QtInfoMsg)

namespace OCC {

GetMetadataApiJob::GetMetadataApiJob(const AccountPtr& account,
                                    const QByteArray& fileId,
                                    QObject* parent)
: AbstractNetworkJob(account, baseUrl() + QStringLiteral("meta-data/") + fileId, parent), _fileId(fileId)
{
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
    QJsonParseError error;
    auto json = QJsonDocument::fromJson(reply()->readAll(), &error);
    emit jsonReceived(json, reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
    return true;
}

StoreMetaDataApiJob::StoreMetaDataApiJob(const AccountPtr& account,
                                                 const QByteArray& fileId,
                                                 const QByteArray& b64Metadata,
                                                 QObject* parent)
: AbstractNetworkJob(account, baseUrl() + QStringLiteral("meta-data/") + fileId, parent), _fileId(fileId), _b64Metadata(b64Metadata)
{
}

void StoreMetaDataApiJob::start()
{
    QNetworkRequest req;
    req.setRawHeader("OCS-APIREQUEST", "true");
    req.setHeader(QNetworkRequest::ContentTypeHeader, QByteArrayLiteral("application/x-www-form-urlencoded"));
    QUrlQuery query;
    query.addQueryItem(QLatin1String("format"), QLatin1String("json"));
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
    int retCode = reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		if (retCode != 200) {
			qCInfo(lcCseJob()) << "error sending the metadata" << path() << errorString() << retCode;
			emit error(_fileId, retCode);
		}

		qCInfo(lcCseJob()) << "Metadata submited to the server successfully";
		emit success(_fileId);
    return true;
}

UpdateMetadataApiJob::UpdateMetadataApiJob(const AccountPtr& account,
                                                 const QByteArray& fileId,
                                                 const QByteArray& b64Metadata,
                                                 const QByteArray& token,
                                                 QObject* parent)
: AbstractNetworkJob(account, baseUrl() + QStringLiteral("meta-data/") + fileId, parent),
_fileId(fileId),
_b64Metadata(b64Metadata),
_token(token)
{
}

void UpdateMetadataApiJob::start()
{
    QNetworkRequest req;
    req.setRawHeader("OCS-APIREQUEST", "true");
    req.setHeader(QNetworkRequest::ContentTypeHeader, QByteArrayLiteral("application/x-www-form-urlencoded"));

    QUrlQuery urlQuery;
    urlQuery.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
    urlQuery.addQueryItem(QStringLiteral("e2e-token"), _token);

    QUrl url = Utility::concatUrlPath(account()->url(), path());
    url.setQuery(urlQuery);

    QUrlQuery params;
    params.addQueryItem("metaData",QUrl::toPercentEncoding(_b64Metadata));
    params.addQueryItem("e2e-token", _token);

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
		}

		qCInfo(lcCseJob()) << "Metadata submited to the server successfully";
		emit success(_fileId);
    return true;
}

UnlockEncryptFolderApiJob::UnlockEncryptFolderApiJob(const AccountPtr& account,
                                                 const QByteArray& fileId,
                                                 const QByteArray& token,
                                                 QObject* parent)
: AbstractNetworkJob(account, baseUrl() + QStringLiteral("lock/") + fileId, parent), _fileId(fileId), _token(token)
{
}

void UnlockEncryptFolderApiJob::start()
{
    QNetworkRequest req;
    req.setRawHeader("OCS-APIREQUEST", "true");
    req.setRawHeader("e2e-token", _token);

    QUrl url = Utility::concatUrlPath(account()->url(), path());
    sendRequest("DELETE", url, req);

    AbstractNetworkJob::start();
    qCInfo(lcCseJob()) << "Starting the request to unlock.";
}

bool UnlockEncryptFolderApiJob::finished()
{
    int retCode = reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (retCode != 200) {
        qCInfo(lcCseJob()) << "error unlocking file" << path() << errorString() << retCode;
        qCInfo(lcCseJob()) << "Full Error Log" << reply()->readAll();
        emit error(_fileId, retCode);
        return true;
    }
    emit success(_fileId);
    return true;
}



DeleteMetadataApiJob::DeleteMetadataApiJob(const AccountPtr& account,
                                                  const QByteArray& fileId,
                                                 QObject* parent)
: AbstractNetworkJob(account, baseUrl() + QStringLiteral("meta-data/") + fileId, parent), _fileId(fileId)
{
}

void DeleteMetadataApiJob::start()
{
    QNetworkRequest req;
    req.setRawHeader("OCS-APIREQUEST", "true");

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

LockEncryptFolderApiJob::LockEncryptFolderApiJob(const AccountPtr& account, const QByteArray& fileId, QObject* parent)
: AbstractNetworkJob(account, baseUrl() + QStringLiteral("lock/") + fileId, parent), _fileId(fileId)
{
}

void LockEncryptFolderApiJob::start()
{
    QNetworkRequest req;
    req.setRawHeader("OCS-APIREQUEST", "true");
    QUrlQuery query;
    query.addQueryItem(QLatin1String("format"), QLatin1String("json"));
    QUrl url = Utility::concatUrlPath(account()->url(), path());
    url.setQuery(query);

    qCInfo(lcCseJob()) << "locking the folder with id" << _fileId << "as encrypted";
    sendRequest("POST", url, req);
    AbstractNetworkJob::start();
}

bool LockEncryptFolderApiJob::finished()
{
    int retCode = reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (retCode != 200) {
        qCInfo(lcCseJob()) << "error locking file" << path() << errorString() << retCode;
        emit error(_fileId, retCode);
        return true;
    }

    QJsonParseError error;
    auto json = QJsonDocument::fromJson(reply()->readAll(), &error);
    auto obj = json.object().toVariantMap();
    auto token = obj["ocs"].toMap()["data"].toMap()["e2e-token"].toByteArray();
    qCInfo(lcCseJob()) << "got json:" << token;

    //TODO: Parse the token and submit.
    emit success(_fileId, token);
    return true;
}

SetEncryptionFlagApiJob::SetEncryptionFlagApiJob(const AccountPtr& account, const QByteArray& fileId, FlagAction flagAction, QObject* parent)
: AbstractNetworkJob(account, baseUrl() + QStringLiteral("encrypted/") + fileId, parent), _fileId(fileId), _flagAction(flagAction)
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
        emit error(_fileId, retCode);
    }
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

    qCInfo(lcStorePrivateKeyApiJob) << "Sending the private key" << _privKey.data();
    sendRequest("POST", url, req, &_privKey);
    AbstractNetworkJob::start();
}

bool StorePrivateKeyApiJob::finished()
{
    int retCode = reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (retCode != 200)
        qCInfo(lcStorePrivateKeyApiJob()) << "Sending private key ended with"  << path() << errorString() << retCode;

    QJsonParseError error;
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

    qCInfo(lcSignPublicKeyApiJob) << "Sending the CSR" << _csr.data();
    sendRequest("POST", url, req, &_csr);
    AbstractNetworkJob::start();
}

bool SignPublicKeyApiJob::finished()
{
    qCInfo(lcStorePrivateKeyApiJob()) << "Sending CSR ended with"  << path() << errorString() << reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute);

    QJsonParseError error;
    auto json = QJsonDocument::fromJson(reply()->readAll(), &error);
    emit jsonReceived(json, reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
    return true;
}

}
