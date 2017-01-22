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
#include <QSslCipher>
#include <QBuffer>
#include <QXmlStreamReader>
#include <QStringList>
#include <QStack>
#include <QTimer>
#include <QMutex>
#include <QDebug>
#include <QCoreApplication>
#include <QPixmap>

#include "json.h"

#include "networkjobs.h"
#include "account.h"
#include "owncloudpropagator.h"

#include "creds/abstractcredentials.h"

namespace OCC {

RequestEtagJob::RequestEtagJob(AccountPtr account, const QString &path, QObject *parent)
    : AbstractNetworkJob(account, path, parent)
{
}

void RequestEtagJob::start()
{
    QNetworkRequest req;
    if (_account && _account->rootEtagChangesNotOnlySubFolderEtags()) {
        // Fixed from 8.1 https://github.com/owncloud/client/issues/3730
        req.setRawHeader("Depth", "0");
    } else {
        // Let's always request all entries inside a directory. There are/were bugs in the server
        // where a root or root-folder ETag is not updated when its contents change. We work around
        // this by concatenating the ETags of the root and its contents.
        req.setRawHeader("Depth", "1");
        // See https://github.com/owncloud/core/issues/5255 and others
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

MkColJob::MkColJob(AccountPtr account, const QString &path, QObject *parent)
    : AbstractNetworkJob(account, path, parent)
{
}

MkColJob::MkColJob(AccountPtr account, const QUrl &url,
                   const QMap<QByteArray, QByteArray> &extraHeaders, QObject *parent)
    : AbstractNetworkJob(account, QString(), parent), _url(url), _extraHeaders(extraHeaders)
{
}

void MkColJob::start()
{
   // add 'Content-Length: 0' header (see https://github.com/owncloud/client/issues/3256)
   QNetworkRequest req;
   req.setRawHeader("Content-Length", "0");
   for(auto it = _extraHeaders.constBegin(); it != _extraHeaders.constEnd(); ++it) {
        req.setRawHeader(it.key(), it.value());
   }

   // assumes ownership
   QNetworkReply *reply = _url.isValid() ?  davRequest("MKCOL", _url, req)
        : davRequest("MKCOL", path(), req);
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
// supposed to read <D:collection> when pointing to <D:resourcetype><D:collection></D:resourcetype>..
static QString readContentsAsString(QXmlStreamReader &reader) {
    QString result;
    int level = 0;
    do {
        QXmlStreamReader::TokenType type = reader.readNext();
        if (type == QXmlStreamReader::StartElement) {
            level++;
            result += "<" + reader.name().toString() + ">";
        } else if (type == QXmlStreamReader::Characters) {
            result += reader.text();
        } else if (type == QXmlStreamReader::EndElement) {
            level--;
            if (level < 0) {
                break;
            }
            result += "</" + reader.name().toString() + ">";
        }

    } while (!reader.atEnd());
    return result;
}


LsColXMLParser::LsColXMLParser()
{

}

bool LsColXMLParser::parse( const QByteArray& xml, QHash<QString, qint64> *sizes, const QString& expectedPath)
{
    // Parse DAV response
    QXmlStreamReader reader(xml);
    reader.addExtraNamespaceDeclaration(QXmlStreamNamespaceDeclaration("d", "DAV:"));

    QStringList folders;
    QString currentHref;
    QMap<QString, QString> currentTmpProperties;
    QMap<QString, QString> currentHttp200Properties;
    bool currentPropsHaveHttp200 = false;
    bool insidePropstat = false;
    bool insideProp = false;
    bool insideMultiStatus = false;

    while (!reader.atEnd()) {
        QXmlStreamReader::TokenType type = reader.readNext();
        QString name = reader.name().toString();
        // Start elements with DAV:
        if (type == QXmlStreamReader::StartElement && reader.namespaceUri() == QLatin1String("DAV:")) {
            if (name == QLatin1String("href")) {
                // We don't use URL encoding in our request URL (which is the expected path) (QNAM will do it for us)
                // but the result will have URL encoding..
                QString hrefString = QString::fromUtf8(QByteArray::fromPercentEncoding(reader.readElementText().toUtf8()));
                if (!hrefString.startsWith(expectedPath)) {
                    qDebug() << "Invalid href" << hrefString << "expected starting with" << expectedPath;
                    return false;
                }
                currentHref = hrefString;
            } else if (name == QLatin1String("response")) {
            } else if (name == QLatin1String("propstat")) {
                insidePropstat = true;
            } else if (name == QLatin1String("status") && insidePropstat) {
                QString httpStatus = reader.readElementText();
                if (httpStatus.startsWith("HTTP/1.1 200")) {
                    currentPropsHaveHttp200 = true;
                } else {
                    currentPropsHaveHttp200 = false;
                }
            } else if (name == QLatin1String("prop")) {
                insideProp = true;
                continue;
            } else if (name == QLatin1String("multistatus")) {
                insideMultiStatus = true;
                continue;
            }
        }

        if (type == QXmlStreamReader::StartElement && insidePropstat && insideProp) {
            // All those elements are properties
            QString propertyContent = readContentsAsString(reader);
            if (name == QLatin1String("resourcetype") && propertyContent.contains("collection")) {
                folders.append(currentHref);
            } else if (name == QLatin1String("size")) {
                bool ok = false;
                auto s = propertyContent.toLongLong(&ok);
                if (ok && sizes) {
                    sizes->insert(currentHref, s);
                }
            }
            currentTmpProperties.insert(reader.name().toString(), propertyContent);
        }

        // End elements with DAV:
        if (type == QXmlStreamReader::EndElement) {
            if (reader.namespaceUri() == QLatin1String("DAV:")) {
                if (reader.name() == "response") {
                    if (currentHref.endsWith('/')) {
                        currentHref.chop(1);
                    }
                    emit directoryListingIterated(currentHref, currentHttp200Properties);
                    currentHref.clear();
                    currentHttp200Properties.clear();
                } else if (reader.name() == "propstat") {
                    insidePropstat = false;
                    if (currentPropsHaveHttp200) {
                        currentHttp200Properties = QMap<QString,QString>(currentTmpProperties);
                    }
                    currentTmpProperties.clear();
                    currentPropsHaveHttp200 = false;
                } else if (reader.name() == "prop") {
                    insideProp = false;
                }
            }
        }
    }

    if (reader.hasError()) {
        // XML Parser error? Whatever had been emitted before will come as directoryListingIterated
        qDebug() << "ERROR" << reader.errorString() << xml;
        return false;
    } else if (!insideMultiStatus) {
        qDebug() << "ERROR no WebDAV response?" << xml;
        return false;
    } else {
        emit directoryListingSubfolders(folders);
        emit finishedWithoutError();
    }
    return true;

}

/*********************************************************************************************/

LsColJob::LsColJob(AccountPtr account, const QString &path, QObject *parent)
    : AbstractNetworkJob(account, path, parent)
{
}

LsColJob::LsColJob(AccountPtr account, const QUrl &url, QObject *parent)
    : AbstractNetworkJob(account, QString(), parent), _url(url)
{
}

void LsColJob::setProperties(QList<QByteArray> properties)
{
    _properties = properties;
}

QList<QByteArray> LsColJob::properties() const
{
    return _properties;
}

void LsColJob::start()
{
    QList<QByteArray> properties = _properties;

    if (properties.isEmpty()) {
        qWarning() << "Propfind with no properties!";
    }
    QByteArray propStr;
    foreach (const QByteArray &prop, properties) {
        if (prop.contains(':')) {
            int colIdx = prop.lastIndexOf(":");
            auto ns = prop.left(colIdx);
            if (ns == "http://owncloud.org/ns") {
                propStr += "    <oc:" + prop.mid(colIdx+1) + " />\n";
            } else {
                propStr += "    <" + prop.mid(colIdx+1) + " xmlns=\"" + ns + "\" />\n";
            }
        } else {
            propStr += "    <d:" + prop + " />\n";
        }
    }

    QNetworkRequest req;
    req.setRawHeader("Depth", "1");
    QByteArray xml("<?xml version=\"1.0\" ?>\n"
                   "<d:propfind xmlns:d=\"DAV:\" xmlns:oc=\"http://owncloud.org/ns\">\n"
                   "  <d:prop>\n"
                   + propStr +
                   "  </d:prop>\n"
                   "</d:propfind>\n");
    QBuffer *buf = new QBuffer(this);
    buf->setData(xml);
    buf->open(QIODevice::ReadOnly);
    QNetworkReply *reply = _url.isValid() ?  davRequest("PROPFIND", _url, req, buf)
        : davRequest("PROPFIND", path(), req, buf);
    buf->setParent(reply);
    setReply(reply);
    setupConnections(reply);
    AbstractNetworkJob::start();
}

// TODO: Instead of doing all in this slot, we should iteratively parse in readyRead(). This
// would allow us to be more asynchronous in processing while data is coming from the network,
// not all in one big blob at the end.
bool LsColJob::finished()
{
    QString contentType = reply()->header(QNetworkRequest::ContentTypeHeader).toString();
    int httpCode = reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (httpCode == 207 && contentType.contains("application/xml; charset=utf-8")) {
        LsColXMLParser parser;
        connect( &parser, SIGNAL(directoryListingSubfolders(const QStringList&)),
                 this, SIGNAL(directoryListingSubfolders(const QStringList&)) );
        connect( &parser, SIGNAL(directoryListingIterated(const QString&, const QMap<QString,QString>&)),
                 this, SIGNAL(directoryListingIterated(const QString&, const QMap<QString,QString>&)) );
        connect( &parser, SIGNAL(finishedWithError(QNetworkReply *)),
                 this, SIGNAL(finishedWithError(QNetworkReply *)) );
        connect( &parser, SIGNAL(finishedWithoutError()),
                 this, SIGNAL(finishedWithoutError()) );

        QString expectedPath = reply()->request().url().path(); // something like "/owncloud/remote.php/webdav/folder"
        if( !parser.parse( reply()->readAll(), &_sizes, expectedPath ) ) {
            // XML parse error
            emit finishedWithError(reply());
        }
    } else if (httpCode == 207) {
        // wrong content type
        emit finishedWithError(reply());
    } else {
        // wrong HTTP code or any other network error
        emit finishedWithError(reply());
    }

    return true;
}

/*********************************************************************************************/

namespace {
const char statusphpC[] = "status.php";
const char owncloudDirC[] = "owncloud/";
}

CheckServerJob::CheckServerJob(AccountPtr account, QObject *parent)
    : AbstractNetworkJob(account, QLatin1String(statusphpC) , parent)
    , _subdirFallback(false)
{
    setIgnoreCredentialFailure(true);
}

void CheckServerJob::start()
{
    setReply(getRequest(path()));
    setupConnections(reply());
    connect(reply(), SIGNAL(metaDataChanged()), this, SLOT(metaDataChangedSlot()));
    connect(reply(), SIGNAL(encrypted()), this, SLOT(encryptedSlot()));
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

static void mergeSslConfigurationForSslButton(const QSslConfiguration &config, AccountPtr account)
{
    if (config.peerCertificateChain().length() > 0) {
        account->_peerCertificateChain = config.peerCertificateChain();
    }
    if (!config.sessionCipher().isNull()) {
        account->_sessionCipher = config.sessionCipher();
    }
#if QT_VERSION > QT_VERSION_CHECK(5, 2, 0)
    if (config.sessionTicket().length() > 0) {
        account->_sessionTicket = config.sessionTicket();
    }
#endif
}

void CheckServerJob::encryptedSlot()
{
    mergeSslConfigurationForSslButton(reply()->sslConfiguration(), account());
}

void CheckServerJob::metaDataChangedSlot()
{
    account()->setSslConfiguration(reply()->sslConfiguration());
    mergeSslConfigurationForSslButton(reply()->sslConfiguration(), account());
}


bool CheckServerJob::finished()
{
#if QT_VERSION > QT_VERSION_CHECK(5, 2, 0)
    if (reply()->request().url().scheme() == QLatin1String("https")
            && reply()->sslConfiguration().sessionTicket().isEmpty()
            && reply()->error() == QNetworkReply::NoError) {
        qDebug() << "No SSL session identifier / session ticket is used, this might impact sync performance negatively.";
    }
#endif

    mergeSslConfigurationForSslButton(reply()->sslConfiguration(), account());

    // The server installs to /owncloud. Let's try that if the file wasn't found
    // at the original location
    if ((reply()->error() == QNetworkReply::ContentNotFoundError) && (!_subdirFallback)) {
        _subdirFallback = true;
        setPath(QLatin1String(owncloudDirC)+QLatin1String(statusphpC));
        start();
        qDebug() << "Retrying with" << reply()->url();
        return false;
    }

    bool success = false;
    QByteArray body = reply()->peek(4*1024);
    int httpStatus = reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if( body.isEmpty() || httpStatus != 200) {
        qDebug() << "error: status.php replied " << httpStatus << body;
        emit instanceNotFound(reply());
    } else {
        QVariantMap status = QtJson::parse(QString::fromUtf8(body), success).toMap();
        // empty or invalid response
        if (!success || status.isEmpty()) {
            qDebug() << "status.php from server is not valid JSON!" << body << reply()->request().url();
        }

        qDebug() << "status.php returns: " << status << " " << reply()->error() << " Reply: " << reply();
        if( status.contains("installed")
                && status.contains("version")
                && status.contains("versionstring") ) {

            emit instanceFound(reply()->url(), status);
        } else {
            qDebug() << "No proper answer on " << reply()->url();
            emit instanceNotFound(reply());
        }
    }
    return true;
}

/*********************************************************************************************/

PropfindJob::PropfindJob(AccountPtr account, const QString &path, QObject *parent)
    : AbstractNetworkJob(account, path, parent)
{

}

void PropfindJob::start()
{
    QList<QByteArray> properties = _properties;

    if (properties.isEmpty()) {
        qWarning() << "Propfind with no properties!";
    }
    QNetworkRequest req;
    // Always have a higher priority than the propagator because we use this from the UI
    // and really want this to be done first (no matter what internal scheduling QNAM uses).
    // Also possibly useful for avoiding false timeouts.
    req.setPriority(QNetworkRequest::HighPriority);
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
                    items.insert(reader.name().toString(), reader.readElementText(QXmlStreamReader::SkipChildElements));
                } else {
                    curElement.push(reader.name().toString());
                }
            }
            if (type == QXmlStreamReader::EndElement) {
                if(curElement.top() == reader.name()) {
                    curElement.pop();
                }
            }
        }
        if (reader.hasError()) {
            qDebug() << "PROPFIND request XML parser error: " << reader.errorString();
            emit finishedWithError(reply());
        } else {
            emit result(items);
        }
    } else {
        qDebug() << "PROPFIND request *not* successful, http result code is" << http_result_code
                 << (http_result_code == 302 ? reply()->header(QNetworkRequest::LocationHeader).toString()  : QLatin1String(""));
        emit finishedWithError(reply());
    }
    return true;
}

/*********************************************************************************************/

AvatarJob::AvatarJob(AccountPtr account, QObject *parent)
    : AbstractNetworkJob(account, QString(), parent)
{
    _avatarUrl = Utility::concatUrlPath(account->url(), QString("remote.php/dav/avatars/%1/128.png").arg(account->davUser()));
}

void AvatarJob::start()
{
    QNetworkRequest req;
    setReply(davRequest("GET", _avatarUrl, req));
    setupConnections(reply());
    AbstractNetworkJob::start();
}

bool AvatarJob::finished()
{
    int http_result_code = reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    QPixmap avPixmap;

    if (http_result_code == 200) {

        QByteArray pngData = reply()->readAll();
        if( pngData.size() ) {

            if( avPixmap.loadFromData(pngData) ) {
                qDebug() << "Retrieved Avatar pixmap!";
            }
        }
    }
    emit(avatarPixmap(avPixmap));
    return true;
}

/*********************************************************************************************/

ProppatchJob::ProppatchJob(AccountPtr account, const QString &path, QObject *parent)
    : AbstractNetworkJob(account, path, parent)
{

}

void ProppatchJob::start()
{
    if (_properties.isEmpty()) {
        qWarning() << "Proppatch with no properties!";
    }
    QNetworkRequest req;

    QByteArray propStr;
    QMapIterator<QByteArray, QByteArray> it(_properties);
    while (it.hasNext()) {
        it.next();
        QByteArray keyName = it.key();
        QByteArray keyNs;
        if (keyName.contains(':')) {
            int colIdx = keyName.lastIndexOf(":");
            keyNs = keyName.left(colIdx);
            keyName = keyName.mid(colIdx+1);
        }

        propStr += "    <" + keyName;
        if (!keyNs.isEmpty()) {
            propStr += " xmlns=\"" + keyNs + "\" ";
        }
        propStr += ">";
        propStr += it.value();
        propStr += "</" + keyName + ">\n";
    }
    QByteArray xml = "<?xml version=\"1.0\" ?>\n"
                     "<d:propertyupdate xmlns:d=\"DAV:\">\n"
                     "  <d:set><d:prop>\n"
                     + propStr +
                     "  </d:prop></d:set>\n"
                     "</d:propertyupdate>\n";

    QBuffer *buf = new QBuffer(this);
    buf->setData(xml);
    buf->open(QIODevice::ReadOnly);
    setReply(davRequest("PROPPATCH", path(), req, buf));
    buf->setParent(reply());
    setupConnections(reply());
    AbstractNetworkJob::start();
}

void ProppatchJob::setProperties(QMap<QByteArray, QByteArray> properties)
{
    _properties = properties;
}

QMap<QByteArray, QByteArray> ProppatchJob::properties() const
{
    return _properties;
}

bool ProppatchJob::finished()
{
    int http_result_code = reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (http_result_code == 207) {
        emit success();
    } else {
        qDebug() << "PROPPATCH request *not* successful, http result code is" << http_result_code
                 << (http_result_code == 302 ? reply()->header(QNetworkRequest::LocationHeader).toString()  : QLatin1String(""));
        emit finishedWithError();
    }
    return true;
}

/*********************************************************************************************/

EntityExistsJob::EntityExistsJob(AccountPtr account, const QString &path, QObject *parent)
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

JsonApiJob::JsonApiJob(const AccountPtr &account, const QString& path, QObject* parent): AbstractNetworkJob(account, path, parent)
{ }

void JsonApiJob::addQueryParams(QList< QPair<QString,QString> > params)
{
    _additionalParams = params;
}

void JsonApiJob::start()
{
    QNetworkRequest req;
    req.setRawHeader("OCS-APIREQUEST", "true");
    QUrl url = Utility::concatUrlPath(account()->url(), path());
    QList<QPair<QString, QString> > params = _additionalParams;
    params << qMakePair(QString::fromLatin1("format"), QString::fromLatin1("json"));
    url.setQueryItems(params);
    setReply(davRequest("GET", url, req));
    setupConnections(reply());
    AbstractNetworkJob::start();
}

bool JsonApiJob::finished()
{
    int statusCode = 0;

    if (reply()->error() != QNetworkReply::NoError) {
        qWarning() << "Network error: " << path() << reply()->errorString() << reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        emit jsonReceived(QVariantMap(), statusCode);
        return true;
    }

    QString jsonStr = QString::fromUtf8(reply()->readAll());
    if( jsonStr.contains( "<?xml version=\"1.0\"?>") ) {
        QRegExp rex("<statuscode>(\\d+)</statuscode>");
        if( jsonStr.contains(rex) ) {
            // this is a error message coming back from ocs.
            statusCode = rex.cap(1).toInt();
        }

    } else {
        QRegExp rex("\"statuscode\":(\\d+),");
        // example: "{"ocs":{"meta":{"status":"ok","statuscode":100,"message":null},"data":{"version":{"major":8,"minor":"... (504)
        if( jsonStr.contains(rex) ) {
            statusCode = rex.cap(1).toInt();
        }
    }

    bool success = false;
    QVariantMap json = QtJson::parse(jsonStr, success).toMap();
    // empty or invalid response
    if (!success || json.isEmpty()) {
        qWarning() << "invalid JSON!" << jsonStr;
        emit jsonReceived(QVariantMap(), statusCode);
        return true;
    }

    emit jsonReceived(json, statusCode);
    return true;
}

} // namespace OCC
