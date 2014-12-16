/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
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

#include <QNetworkRequest>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslConfiguration>
#include <QBuffer>
#include <QXmlStreamReader>
#include <QStringList>
#include <QStack>
#include <QTimer>
#include <QMutex>
#include <QDebug>
#include <QCoreApplication>

#include "json.h"

#include "networkjobs.h"
#include "account.h"
#include "owncloudpropagator.h"

#include "creds/credentialsfactory.h"
#include "creds/abstractcredentials.h"

Q_DECLARE_METATYPE(QTimer*)

namespace OCC {

bool AbstractNetworkJob::preOc7WasDetected = false;

AbstractNetworkJob::AbstractNetworkJob(Account *account, const QString &path, QObject *parent)
    : QObject(parent)
    , _duration(0)
    , _timedout(false)
    , _followRedirects(false)
    , _ignoreCredentialFailure(false)
    , _reply(0)
    , _account(account)
    , _path(path)
    , _redirectCount(0)
{
    _timer.setSingleShot(true);
    _timer.setInterval(OwncloudPropagator::httpTimeout() * 1000); // default to 5 minutes.
    connect(&_timer, SIGNAL(timeout()), this, SLOT(slotTimeout()));

    connect(this, SIGNAL(networkActivity()), SLOT(resetTimeout()));

    // Network activity on the propagator jobs (GET/PUT) keeps all requests alive.
    // This is a workaround for OC instances which only support one
    // parallel up and download
    if (_account) {
        connect(_account, SIGNAL(propagatorNetworkActivity()), SLOT(resetTimeout()));
    }
}

void AbstractNetworkJob::setReply(QNetworkReply *reply)
{
    if (_reply) {
        _reply->deleteLater();
    }
    reply->setProperty("doNotHandleAuth", true);
    _reply = reply;
}

void AbstractNetworkJob::setTimeout(qint64 msec)
{
    qDebug() << Q_FUNC_INFO << msec;

    _timer.start(msec);
}

void AbstractNetworkJob::resetTimeout()
{
    qint64 interval = _timer.interval();
    _timer.stop();
    _timer.start(interval);
}

void AbstractNetworkJob::setIgnoreCredentialFailure(bool ignore)
{
    _ignoreCredentialFailure = ignore;
}

void AbstractNetworkJob::setPath(const QString &path)
{
    _path = path;
}

void AbstractNetworkJob::setupConnections(QNetworkReply *reply)
{
    connect(reply, SIGNAL(finished()), SLOT(slotFinished()));
#if QT_VERSION >= QT_VERSION_CHECK(5, 1, 0)
    connect(reply, SIGNAL(encrypted()), SIGNAL(networkActivity()));
#endif
    connect(reply->manager(), SIGNAL(proxyAuthenticationRequired(QNetworkProxy,QAuthenticator*)), SIGNAL(networkActivity()));
    connect(reply, SIGNAL(sslErrors(QList<QSslError>)), SIGNAL(networkActivity()));
    connect(reply, SIGNAL(metaDataChanged()), SIGNAL(networkActivity()));
    connect(reply, SIGNAL(downloadProgress(qint64,qint64)), SIGNAL(networkActivity()));
    connect(reply, SIGNAL(uploadProgress(qint64,qint64)), SIGNAL(networkActivity()));
}

QNetworkReply* AbstractNetworkJob::addTimer(QNetworkReply *reply)
{
    reply->setProperty("timer", QVariant::fromValue(&_timer));
    return reply;
}

QNetworkReply* AbstractNetworkJob::davRequest(const QByteArray &verb, const QString &relPath,
                                              QNetworkRequest req, QIODevice *data)
{
    return addTimer(_account->davRequest(verb, relPath, req, data));
}

QNetworkReply *AbstractNetworkJob::davRequest(const QByteArray &verb, const QUrl &url, QNetworkRequest req, QIODevice *data)
{
    return addTimer(_account->davRequest(verb, url, req, data));
}

QNetworkReply* AbstractNetworkJob::getRequest(const QString &relPath)
{
    return addTimer(_account->getRequest(relPath));
}

QNetworkReply *AbstractNetworkJob::getRequest(const QUrl &url)
{
    return addTimer(_account->getRequest(url));
}

QNetworkReply *AbstractNetworkJob::headRequest(const QString &relPath)
{
    return addTimer(_account->headRequest(relPath));
}

QNetworkReply *AbstractNetworkJob::headRequest(const QUrl &url)
{
    return addTimer(_account->headRequest(url));
}

void AbstractNetworkJob::slotFinished()
{
    _timer.stop();

    if( _reply->error() != QNetworkReply::NoError ) {
        qDebug() << Q_FUNC_INFO << _reply->error() << _reply->errorString();
        if (_reply->error() == QNetworkReply::ProxyAuthenticationRequiredError) {
            qDebug() << Q_FUNC_INFO << _reply->rawHeader("Proxy-Authenticate");
        }
        emit networkError(_reply);
    }

    // get the Date timestamp from reply
    _responseTimestamp = QString::fromAscii(_reply->rawHeader("Date"));
    _duration = _durationTimer.elapsed();


    if (_followRedirects) {
        // ### the qWarnings here should be exported via displayErrors() so they
        // ### can be presented to the user if the job executor has a GUI
        QUrl requestedUrl = reply()->request().url();
        QUrl redirectUrl = reply()->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
        if (!redirectUrl.isEmpty()) {
            _redirectCount++;
            if (requestedUrl.scheme() == QLatin1String("https") &&
                    redirectUrl.scheme() == QLatin1String("http")) {
                qWarning() << this << "HTTPS->HTTP downgrade detected!";
            } else if (requestedUrl == redirectUrl || _redirectCount >= maxRedirects()) {
                qWarning() << this << "Redirect loop detected!";
            } else {
                resetTimeout();
                setReply(getRequest(redirectUrl));
                setupConnections(reply());
                return;
            }
        }
    }

    bool discard = finished();
    AbstractCredentials *creds = _account->credentials();
    if (!creds->stillValid(_reply) &&! _ignoreCredentialFailure
            && _account->state() != Account::InvalidCredential) {
        _account->setState(Account::InvalidCredential);

        // invalidate & forget token/password
        // but try to re-sign in.
        connect( creds, SIGNAL(fetched()),
                 qApp, SLOT(slotCredentialsFetched()), Qt::UniqueConnection);
        if (creds->ready()) {
            creds->invalidateAndFetch(_account);
        } else {
            creds->fetch(_account);
        }
    }
    if (discard) {
        deleteLater();
    }
}

quint64 AbstractNetworkJob::duration()
{
    return _duration;
}

QString AbstractNetworkJob::responseTimestamp()
{
    return _responseTimestamp;
}

AbstractNetworkJob::~AbstractNetworkJob()
{
    if (_reply) {
        _reply->deleteLater();
    }
}

void AbstractNetworkJob::start()
{
    _timer.start();
    _durationTimer.start();
    _duration = 0;

    qDebug() << "!!!" << metaObject()->className() << "created for" << account()->url() << "querying" << path();
}

void AbstractNetworkJob::slotTimeout()
{
    _timedout = true;
    qDebug() <<  this << "Timeout";
    if (reply()) {
        reply()->abort();
    } else {
        qDebug() << "reply was NULL";
    }
}

/*********************************************************************************************/

RequestEtagJob::RequestEtagJob(Account *account, const QString &path, QObject *parent)
    : AbstractNetworkJob(account, path, parent)
{
}

void RequestEtagJob::start()
{
    QNetworkRequest req;
    if (path().isEmpty() || path() == QLatin1String("/")) {
        /* For the root directory, we need to query the etags of all the sub directories
         * because, at the time I am writing this comment (Owncloud 5.0.9), the etag of the
         * root directory is not updated when the sub directories changes */
        req.setRawHeader("Depth", "1");
    } else {
        req.setRawHeader("Depth", "0");
    }
    QByteArray xml("<?xml version=\"1.0\" ?>\n"
                   "<d:propfind xmlns:d=\"DAV:\">\n"
                   "  <d:prop>\n"
                   "    <d:getetag/>\n"
                   "  </d:prop>\n"
                   "</d:propfind>\n");
    QBuffer *buf = new QBuffer(this);
    buf->setData(xml);
    buf->open(QIODevice::ReadOnly);
    // assumes ownership
    setReply(davRequest("PROPFIND", path(), req, buf));
    buf->setParent(reply());
    setupConnections(reply());

    if( reply()->error() != QNetworkReply::NoError ) {
        qDebug() << "getting etag: request network error: " << reply()->errorString();
    }
    AbstractNetworkJob::start();
}

bool RequestEtagJob::finished()
{
    if (reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute) == 207) {
        // Parse DAV response
        QXmlStreamReader reader(reply());
        reader.addExtraNamespaceDeclaration(QXmlStreamNamespaceDeclaration("d", "DAV:"));
        QString etag;
        while (!reader.atEnd()) {
            QXmlStreamReader::TokenType type = reader.readNext();
            if (type == QXmlStreamReader::StartElement &&
                    reader.namespaceUri() == QLatin1String("DAV:")) {
                QString name = reader.name().toString();
                if (name == QLatin1String("getetag")) {
                    etag += reader.readElementText();
                }
            }
        }
        emit etagRetreived(etag);
    }
    return true;
}

/*********************************************************************************************/

MkColJob::MkColJob(Account *account, const QString &path, QObject *parent)
    : AbstractNetworkJob(account, path, parent)
{
}

void MkColJob::start()
{
    // assumes ownership
   QNetworkReply *reply = davRequest("MKCOL", path());
   setReply(reply);
   setupConnections(reply);
   AbstractNetworkJob::start();
}

bool MkColJob::finished()
{
    emit finished(reply()->error());
    return true;
}

/*********************************************************************************************/

LsColJob::LsColJob(Account *account, const QString &path, QObject *parent)
    : AbstractNetworkJob(account, path, parent)
{
}

void LsColJob::start()
{
    QNetworkRequest req;
    req.setRawHeader("Depth", "1");
    QByteArray xml("<?xml version=\"1.0\" ?>\n"
                   "<d:propfind xmlns:d=\"DAV:\">\n"
                   "  <d:prop>\n"
                   "    <d:resourcetype/>\n"
                   "  </d:prop>\n"
                   "</d:propfind>\n");
    QBuffer *buf = new QBuffer(this);
    buf->setData(xml);
    buf->open(QIODevice::ReadOnly);
    QNetworkReply *reply = davRequest("PROPFIND", path(), req, buf);
    buf->setParent(reply);
    setReply(reply);
    setupConnections(reply);
    AbstractNetworkJob::start();
}

bool LsColJob::finished()
{
    if (reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute) == 207) {
        // Parse DAV response
        QXmlStreamReader reader(reply());
        reader.addExtraNamespaceDeclaration(QXmlStreamNamespaceDeclaration("d", "DAV:"));

        QStringList folders;
        QString currentItem;

        while (!reader.atEnd()) {
            QXmlStreamReader::TokenType type = reader.readNext();
            if (type == QXmlStreamReader::StartElement &&
                    reader.namespaceUri() == QLatin1String("DAV:")) {
                QString name = reader.name().toString();
                if (name == QLatin1String("href")) {
                    currentItem = reader.readElementText();
                } else if (name == QLatin1String("collection") &&
                           !currentItem.isEmpty()) {
                    folders.append(QUrl::fromEncoded(currentItem.toLatin1()).path());
                    currentItem.clear();
                }
            }
        }
        emit directoryListing(folders);
    }
    return true;
}

/*********************************************************************************************/

namespace {
const char statusphpC[] = "status.php";
const char owncloudDirC[] = "owncloud/";
}

CheckServerJob::CheckServerJob(Account *account, QObject *parent)
    : AbstractNetworkJob(account, QLatin1String(statusphpC) , parent)
    , _subdirFallback(false)
{
	_followRedirects = true;
    setIgnoreCredentialFailure(true);
}

void CheckServerJob::start()
{
    setReply(getRequest(path()));
    setupConnections(reply());
    AbstractNetworkJob::start();
}

void CheckServerJob::slotTimeout()
{
    qDebug() << "TIMEOUT" << Q_FUNC_INFO;
    if (reply() && reply()->isRunning()) {
        emit timeout(reply()->url());
    } else if (!reply()) {
        qDebug() << Q_FUNC_INFO << "Timeout even there was no reply?";
    }
    deleteLater();
}

QString CheckServerJob::version(const QVariantMap &info)
{
    return info.value(QLatin1String("version")).toString();
}

QString CheckServerJob::versionString(const QVariantMap &info)
{
    return info.value(QLatin1String("versionstring")).toString();
}

bool CheckServerJob::installed(const QVariantMap &info)
{
    return info.value(QLatin1String("installed")).toBool();
}

bool CheckServerJob::finished()
{
    account()->setSslConfiguration(reply()->sslConfiguration());

    // The serverInstalls to /owncloud. Let's try that if the file wasn't found
    // at the original location
    if ((reply()->error() == QNetworkReply::ContentNotFoundError) && (!_subdirFallback)) {
        _subdirFallback = true;
        setPath(QLatin1String(owncloudDirC)+QLatin1String(statusphpC));
        start();
        qDebug() << "Retrying with" << reply()->url();
        return false;
    }

    bool success = false;
    QByteArray body = reply()->readAll();
    int httpStatus = reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if( body.isEmpty() || httpStatus != 200) {
        qDebug() << "error: status.php replied " << httpStatus << body;
        emit instanceNotFound(reply());
    } else {
        QVariantMap status = QtJson::parse(QString::fromUtf8(body), success).toMap();
        // empty or invalid response
        if (!success || status.isEmpty()) {
            qDebug() << "status.php from server is not valid JSON!";
        }

        qDebug() << "status.php returns: " << status << " " << reply()->error() << " Reply: " << reply();
        if( status.contains("installed")
                && status.contains("version")
                && status.contains("versionstring") ) {

            QString versionString = status.value("version").toString();
            if (versionString.contains('.') && versionString.split('.')[0].toInt() < 7) {
                AbstractNetworkJob::preOc7WasDetected = true;
            }

            emit instanceFound(reply()->url(), status);
        } else {
            qDebug() << "No proper answer on " << reply()->url();
            emit instanceNotFound(reply());
        }
    }
    return true;
}

/*********************************************************************************************/

PropfindJob::PropfindJob(Account *account, const QString &path, QObject *parent)
    : AbstractNetworkJob(account, path, parent)
{

}

void PropfindJob::start()
{
    QList<QByteArray> properties = _properties;

    if (properties.isEmpty()) {
        properties << "allprop";
    }
    QNetworkRequest req;
    req.setRawHeader("Depth", "0");
    QByteArray propStr;
    foreach (const QByteArray &prop, properties) {
        if (prop.contains(':')) {
            int colIdx = prop.lastIndexOf(":");
            propStr += "    <" + prop.mid(colIdx+1) + " xmlns=\"" + prop.left(colIdx) + "\" />\n";
        } else {
            propStr += "    <d:" + prop + " />\n";
        }
    }
    QByteArray xml = "<?xml version=\"1.0\" ?>\n"
                     "<d:propfind xmlns:d=\"DAV:\">\n"
                     "  <d:prop>\n"
                     + propStr +
                     "  </d:prop>\n"
                     "</d:propfind>\n";

    QBuffer *buf = new QBuffer(this);
    buf->setData(xml);
    buf->open(QIODevice::ReadOnly);
    setReply(davRequest("PROPFIND", path(), req, buf));
    buf->setParent(reply());
    setupConnections(reply());
    AbstractNetworkJob::start();
}

void PropfindJob::setProperties(QList<QByteArray> properties)
{
    _properties = properties;
}

QList<QByteArray> PropfindJob::properties() const
{
    return _properties;
}

bool PropfindJob::finished()
{
    int http_result_code = reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (http_result_code == 207) {
        // Parse DAV response
        QXmlStreamReader reader(reply());
        reader.addExtraNamespaceDeclaration(QXmlStreamNamespaceDeclaration("d", "DAV:"));

        QVariantMap items;
        // introduced to nesting is ignored
        QStack<QString> curElement;

        while (!reader.atEnd()) {
            QXmlStreamReader::TokenType type = reader.readNext();
            if (type == QXmlStreamReader::StartElement) {
                if (!curElement.isEmpty() && curElement.top() == QLatin1String("prop")) {
                    items.insert(reader.name().toString(), reader.readElementText());
                }
                curElement.push(reader.name().toString());
            }
            if (type == QXmlStreamReader::EndElement) {
                if(curElement.top() == reader.name()) {
                    curElement.pop();
                }
            }
        }
        emit result(items);
    } else {
        qDebug() << "Quota request *not* successful, http result code is" << http_result_code
                 << (http_result_code == 302 ? reply()->header(QNetworkRequest::LocationHeader).toString()  : QLatin1String(""));
        emit finishedWithError();
    }
    return true;
}

/*********************************************************************************************/

EntityExistsJob::EntityExistsJob(Account *account, const QString &path, QObject *parent)
    : AbstractNetworkJob(account, path, parent)
{
}

void EntityExistsJob::start()
{
    setReply(headRequest(path()));
    setupConnections(reply());
    AbstractNetworkJob::start();
}

bool EntityExistsJob::finished()
{
    emit exists(reply());
    return true;
}

/*********************************************************************************************/

CheckQuotaJob::CheckQuotaJob(Account *account, const QString &path, QObject *parent)
    : AbstractNetworkJob(account, path, parent)
{
}

void CheckQuotaJob::start()
{
    QNetworkRequest req;
    req.setRawHeader("Depth", "0");
    QByteArray xml("<?xml version=\"1.0\" ?>\n"
                   "<d:propfind xmlns:d=\"DAV:\">\n"
                   "  <d:prop>\n"
                   "    <d:quota-available-bytes/>\n"
                   "    <d:quota-used-bytes/>\n"
                   "  </d:prop>\n"
                   "</d:propfind>\n");
    QBuffer *buf = new QBuffer(this);
    buf->setData(xml);
    buf->open(QIODevice::ReadOnly);
    // assumes ownership
    setReply(davRequest("PROPFIND", path(), req, buf));
    buf->setParent(reply());
    setupConnections(reply());
    AbstractNetworkJob::start();
}

bool CheckQuotaJob::finished()
{
    if (reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute) == 207) {
        // Parse DAV response
        QXmlStreamReader reader(reply());
        reader.addExtraNamespaceDeclaration(QXmlStreamNamespaceDeclaration("d", "DAV:"));
        qint64 quotaAvailableBytes = 0;
        qint64 quotaUsedBytes = 0;
        while (!reader.atEnd()) {
            QXmlStreamReader::TokenType type = reader.readNext();
            if (type == QXmlStreamReader::StartElement &&
                    reader.namespaceUri() == QLatin1String("DAV:")) {
                QString name = reader.name().toString();
                if (name == QLatin1String("quota-available-bytes")) {
                    // I have seen the server returning frational bytes:
                    //   <d:quota-available-bytes>1374532061.2</d:quota-available-bytes>
                    quotaAvailableBytes = reader.readElementText().toDouble();
                } else if (name == QLatin1String("quota-used-bytes")) {
                    quotaUsedBytes = reader.readElementText().toDouble();
                }
            }
        }
        qint64 total = quotaUsedBytes + quotaAvailableBytes;
        emit quotaRetrieved(total, quotaUsedBytes);
    }
    return true;
}

NetworkJobTimeoutPauser::NetworkJobTimeoutPauser(QNetworkReply *reply)
{
    _timer = reply->property("timer").value<QTimer*>();
    if(!_timer.isNull()) {
        _timer->stop();
    }
}

NetworkJobTimeoutPauser::~NetworkJobTimeoutPauser()
{
    if(!_timer.isNull()) {
        _timer->start();
    }
}

} // namespace OCC
