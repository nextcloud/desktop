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

#include <QLoggingCategory>
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
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#ifndef TOKEN_AUTH_ONLY
#include <QPainter>
#include <QPainterPath>
#endif

#include "networkjobs.h"
#include "account.h"
#include "owncloudpropagator.h"

#include "creds/abstractcredentials.h"
#include "creds/httpcredentials.h"
#include "theme.h"


namespace OCC {

Q_LOGGING_CATEGORY(lcEtagJob, "sync.networkjob.etag", QtInfoMsg)
Q_LOGGING_CATEGORY(lcLsColJob, "sync.networkjob.lscol", QtInfoMsg)
Q_LOGGING_CATEGORY(lcCheckServerJob, "sync.networkjob.checkserver", QtInfoMsg)
Q_LOGGING_CATEGORY(lcPropfindJob, "sync.networkjob.propfind", QtInfoMsg)
Q_LOGGING_CATEGORY(lcAvatarJob, "sync.networkjob.avatar", QtInfoMsg)
Q_LOGGING_CATEGORY(lcMkColJob, "sync.networkjob.mkcol", QtInfoMsg)
Q_LOGGING_CATEGORY(lcProppatchJob, "sync.networkjob.proppatch", QtInfoMsg)
Q_LOGGING_CATEGORY(lcJsonApiJob, "sync.networkjob.jsonapi", QtInfoMsg)
Q_LOGGING_CATEGORY(lcDetermineAuthTypeJob, "sync.networkjob.determineauthtype", QtInfoMsg)

QByteArray parseEtag(const QByteArray &header)
{
    if (header.isEmpty())
        return QByteArray();
    QByteArray arr = header;

    // Weak E-Tags can appear when gzip compression is on, see #3946
    if (arr.startsWith("W/"))
        arr = arr.mid(2);

    // https://github.com/owncloud/client/issues/1195
    arr.replace("-gzip", "");

    if (arr.length() >= 2 && arr.startsWith('"') && arr.endsWith('"')) {
        arr = arr.mid(1, arr.length() - 2);
    }
    return arr;
}

RequestEtagJob::RequestEtagJob(AccountPtr account, const QString &path, QObject *parent)
    : AbstractNetworkJob(account, path, parent)
{
}

void RequestEtagJob::start()
{
    QNetworkRequest req;
    req.setRawHeader("Depth", "0");

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
    sendRequest("PROPFIND", makeDavUrl(path()), req, buf);
    AbstractNetworkJob::start();
}

bool RequestEtagJob::finished()
{
    qCInfo(lcEtagJob) << "Request Etag of" << reply()->request().url() << "FINISHED WITH STATUS"
                      <<  replyStatusString();

    auto httpCode = reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (httpCode == 207) {
        // Parse DAV response
        QXmlStreamReader reader(reply());
        reader.addExtraNamespaceDeclaration(QXmlStreamNamespaceDeclaration(QStringLiteral("d"), QStringLiteral("DAV:")));
        QByteArray etag;
        while (!reader.atEnd()) {
            QXmlStreamReader::TokenType type = reader.readNext();
            if (type == QXmlStreamReader::StartElement && reader.namespaceUri() == QLatin1String("DAV:")) {
                QString name = reader.name().toString();
                if (name == QLatin1String("getetag")) {
                    auto etagText = reader.readElementText();
                    auto parsedTag = parseEtag(etagText.toUtf8());
                    if (!parsedTag.isEmpty()) {
                        etag += parsedTag;
                    } else {
                        etag += etagText.toUtf8();
                    }
                }
            }
        }
        emit etagRetreived(etag, QDateTime::fromString(QString::fromUtf8(_responseTimestamp), Qt::RFC2822Date));
        emit finishedWithResult(etag);
    } else {
        emit finishedWithResult(HttpError{ httpCode, errorString() });
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
    : AbstractNetworkJob(account, QString(), parent)
    , _url(url)
    , _extraHeaders(extraHeaders)
{
}

void MkColJob::start()
{
    // add 'Content-Length: 0' header (see https://github.com/owncloud/client/issues/3256)
    QNetworkRequest req;
    req.setRawHeader("Content-Length", "0");
    for (auto it = _extraHeaders.constBegin(); it != _extraHeaders.constEnd(); ++it) {
        req.setRawHeader(it.key(), it.value());
    }

    // assumes ownership
    if (_url.isValid()) {
        sendRequest("MKCOL", _url, req);
    } else {
        sendRequest("MKCOL", makeDavUrl(path()), req);
    }
    AbstractNetworkJob::start();
}

bool MkColJob::finished()
{
    qCInfo(lcMkColJob) << "MKCOL of" << reply()->request().url() << "FINISHED WITH STATUS"
                       << replyStatusString();

    if (reply()->error() != QNetworkReply::NoError) {
        Q_EMIT finishedWithError(reply());
    } else {
        Q_EMIT finishedWithoutError();
    }
    return true;
}

/*********************************************************************************************/
// supposed to read <D:collection> when pointing to <D:resourcetype><D:collection></D:resourcetype>..
static QString readContentsAsString(QXmlStreamReader &reader)
{
    QString result;
    int level = 0;
    do {
        QXmlStreamReader::TokenType type = reader.readNext();
        if (type == QXmlStreamReader::StartElement) {
            level++;
            result += QLatin1Char('<') + reader.name().toString() + QLatin1Char('>');
        } else if (type == QXmlStreamReader::Characters) {
            result += reader.text();
        } else if (type == QXmlStreamReader::EndElement) {
            level--;
            if (level < 0) {
                break;
            }
            result += QStringLiteral("</") + reader.name().toString() + QLatin1Char('>');
        }

    } while (!reader.atEnd());
    return result;
}


LsColXMLParser::LsColXMLParser()
{
}

bool LsColXMLParser::parse(const QByteArray &xml, QHash<QString, qint64> *sizes, const QString &expectedPath)
{
    // Parse DAV response
    QXmlStreamReader reader(xml);
    reader.addExtraNamespaceDeclaration(QXmlStreamNamespaceDeclaration(QStringLiteral("d"), QStringLiteral("DAV:")));

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
                    qCWarning(lcLsColJob) << "Invalid href" << hrefString << "expected starting with" << expectedPath;
                    return false;
                }
                currentHref = hrefString;
            } else if (name == QLatin1String("response")) {
            } else if (name == QLatin1String("propstat")) {
                insidePropstat = true;
            } else if (name == QLatin1String("status") && insidePropstat) {
                QString httpStatus = reader.readElementText();
                if (httpStatus.startsWith(QLatin1String("HTTP/1.1 200"))) {
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
            if (name == QLatin1String("resourcetype") && propertyContent.contains(QLatin1String("collection"))) {
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
                if (reader.name() == QLatin1String("response")) {
                    if (currentHref.endsWith(QLatin1Char('/'))) {
                        currentHref.chop(1);
                    }
                    emit directoryListingIterated(currentHref, currentHttp200Properties);
                    currentHref.clear();
                    currentHttp200Properties.clear();
                } else if (reader.name() == QLatin1String("propstat")) {
                    insidePropstat = false;
                    if (currentPropsHaveHttp200) {
                        currentHttp200Properties = std::move(currentTmpProperties);
                    }
                    currentPropsHaveHttp200 = false;
                } else if (reader.name() == QLatin1String("prop")) {
                    insideProp = false;
                }
            }
        }
    }

    if (reader.hasError()) {
        // XML Parser error? Whatever had been emitted before will come as directoryListingIterated
        qCWarning(lcLsColJob) << "ERROR" << reader.errorString() << xml;
        return false;
    } else if (!insideMultiStatus) {
        qCWarning(lcLsColJob) << "ERROR no WebDAV response?" << xml;
        return false;
    } else {
        emit directoryListingSubfolders(folders);
        emit finishedWithoutError();
    }
    return true;
}

/*********************************************************************************************/

LsColJob::LsColJob(AccountPtr account, const QString &path, QObject *parent)
    : AbstractNetworkJob(account, QString(), parent)
    , _url(makeDavUrl(path))
{
    // Always have a higher priority than the propagator because we use this from the UI
    // and really want this to be done first (no matter what internal scheduling QNAM uses).
    // Also possibly useful for avoiding false timeouts.
    setPriority(QNetworkRequest::HighPriority);
}

LsColJob::LsColJob(AccountPtr account, const QUrl &url, QObject *parent)
    : AbstractNetworkJob(account, QString(), parent)
    , _url(url)
{
    // Always have a higher priority than the propagator because we use this from the UI
    // and really want this to be done first (no matter what internal scheduling QNAM uses).
    // Also possibly useful for avoiding false timeouts.
    setPriority(QNetworkRequest::HighPriority);
}

void LsColJob::setProperties(const QList<QByteArray> &properties)
{
    _properties = properties;
}

QList<QByteArray> LsColJob::properties() const
{
    return _properties;
}

void LsColJob::start()
{
    QNetworkRequest req;
    req.setRawHeader(QByteArrayLiteral("Depth"), QByteArrayLiteral("1"));
    startImpl(req);
}

// TODO: Instead of doing all in this slot, we should iteratively parse in readyRead(). This
// would allow us to be more asynchronous in processing while data is coming from the network,
// not all in one big blob at the end.
bool LsColJob::finished()
{
    qCInfo(lcLsColJob) << "LSCOL of" << reply()->request().url() << "FINISHED WITH STATUS"
                       << replyStatusString();

    QString contentType = reply()->header(QNetworkRequest::ContentTypeHeader).toString();
    int httpCode = reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (httpCode == 207 && contentType.contains(QLatin1String("application/xml; charset=utf-8"))) {
        LsColXMLParser parser;
        connect(&parser, &LsColXMLParser::directoryListingSubfolders,
            this, &LsColJob::directoryListingSubfolders);
        connect(&parser, &LsColXMLParser::directoryListingIterated,
            this, &LsColJob::directoryListingIterated);
        connect(&parser, &LsColXMLParser::finishedWithError,
            this, &LsColJob::finishedWithError);
        connect(&parser, &LsColXMLParser::finishedWithoutError,
            this, &LsColJob::finishedWithoutError);

        QString expectedPath = reply()->request().url().path(); // something like "/owncloud/remote.php/webdav/folder"
        if (!parser.parse(reply()->readAll(), &_sizes, expectedPath)) {
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

void LsColJob::startImpl(const QNetworkRequest &req)
{
    if (_properties.isEmpty()) {
        qCWarning(lcLsColJob) << "Propfind with no properties!";
    }
    QByteArray data;
    {
        QTextStream stream(&data, QIODevice::WriteOnly);
        stream.setCodec("UTF-8");
        stream << QByteArrayLiteral("<?xml version=\"1.0\" encoding=\"utf-8\"?>"
                                    "<d:propfind xmlns:d=\"DAV:\">"
                                    "<d:prop>");

        for (const QByteArray &prop : qAsConst(_properties)) {
            const int colIdx = prop.lastIndexOf(':');
            if (colIdx >= 0) {
                stream << QByteArrayLiteral("<") << prop.mid(colIdx + 1) << QByteArrayLiteral(" xmlns=\"") << prop.left(colIdx) << QByteArrayLiteral("\"/>");
            } else {
                stream << QByteArrayLiteral("<d:") << prop << QByteArrayLiteral("/>");
            }
        }
        stream << QByteArrayLiteral("</d:prop>M"
                                    "</d:propfind>\n");
    }

    QBuffer *buf = new QBuffer(this);
    buf->setData(data);
    buf->open(QIODevice::ReadOnly);
    sendRequest(QByteArrayLiteral("PROPFIND"), _url, req, buf);
    AbstractNetworkJob::start();
}

const QHash<QString, qint64> &LsColJob::sizes() const
{
    return _sizes;
}

/*********************************************************************************************/

CheckServerJob::CheckServerJob(AccountPtr account, QObject *parent)
    : AbstractNetworkJob(account, QStringLiteral("status.php"), parent)
{
    setIgnoreCredentialFailure(true);
    setAuthenticationJob(true);
}

void CheckServerJob::start()
{
    _serverUrl = account()->url();
    QNetworkRequest req;
    // don't authenticate the request to a possibly external service
    req.setAttribute(HttpCredentials::DontAddCredentialsAttribute, true);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setRawHeader(QByteArrayLiteral("OC-Connection-Validator"), QByteArrayLiteral("desktop"));
    req.setMaximumRedirectsAllowed(_maxRedirectsAllowed);

    if (_clearCookies && Theme::instance()->connectionValidatorClearCookies()) {
        _account->clearCookieJar();
    }
    sendRequest("GET", Utility::concatUrlPath(_serverUrl, path()), req);
    AbstractNetworkJob::start();
}

void CheckServerJob::setClearCookies(bool clearCookies)
{
    _clearCookies = clearCookies;
}

void CheckServerJob::onTimedOut()
{
    qCWarning(lcCheckServerJob) << "TIMEOUT";
    if (reply() && reply()->isRunning()) {
        emit timeout(reply()->url());
    } else if (!reply()) {
        qCWarning(lcCheckServerJob) << "Timeout even there was no reply?";
    }
    AbstractNetworkJob::onTimedOut();
}

QString CheckServerJob::version(const QJsonObject &info)
{
    return info.value(QLatin1String("version")).toString() + QLatin1Char('-') + info.value(QLatin1String("productname")).toString();
}

QString CheckServerJob::versionString(const QJsonObject &info)
{
    return info.value(QLatin1String("versionstring")).toString();
}

bool CheckServerJob::installed(const QJsonObject &info)
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
    if (config.sessionTicket().length() > 0) {
        account->_sessionTicket = config.sessionTicket();
    }
}

void CheckServerJob::encryptedSlot()
{
    mergeSslConfigurationForSslButton(reply()->sslConfiguration(), account());
}

void CheckServerJob::newReplyHook(QNetworkReply *reply)
{
    connect(reply, &QNetworkReply::metaDataChanged, this, &CheckServerJob::metaDataChangedSlot);
    connect(reply, &QNetworkReply::encrypted, this, &CheckServerJob::encryptedSlot);
    connect(reply, &QNetworkReply::redirected, this, [reply, this] {
        const auto code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (code == 302 || code == 307) {
            _redirectDistinct = false;
        }
    });
}

int CheckServerJob::maxRedirectsAllowed() const
{
    return _maxRedirectsAllowed;
}

void CheckServerJob::setMaxRedirectsAllowed(int maxRedirectsAllowed)
{
    _maxRedirectsAllowed = maxRedirectsAllowed;
}

void CheckServerJob::metaDataChangedSlot()
{
    account()->setSslConfiguration(reply()->sslConfiguration());
    mergeSslConfigurationForSslButton(reply()->sslConfiguration(), account());
}

bool CheckServerJob::finished()
{
    const QUrl targetUrl = reply()->url().adjusted(QUrl::RemoveFilename);
    if (targetUrl.scheme() == QLatin1String("https")
        && reply()->sslConfiguration().sessionTicket().isEmpty()
        && reply()->error() == QNetworkReply::NoError) {
        qCWarning(lcCheckServerJob) << "No SSL session identifier / session ticket is used, this might impact sync performance negatively.";
    }
    if (_serverUrl != targetUrl) {
        if (_redirectDistinct) {
            _serverUrl = targetUrl;
        } else {
            if (_firstTry) {
                qCWarning(lcCheckServerJob) << "Server might have moved, retry";
                _firstTry = false;
                _redirectDistinct = true;
                start();
                return false;
            } else {
                qCWarning(lcCheckServerJob) << "We got a temporary moved server aborting";
                emit instanceNotFound(reply());
                return true;
            }
        }
    }

    mergeSslConfigurationForSslButton(reply()->sslConfiguration(), account());

    const int httpStatus = reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply()->error() == QNetworkReply::TooManyRedirectsError) {
        qCWarning(lcCheckServerJob) << "error:" << reply()->errorString();
        emit instanceNotFound(reply());
    } else if (httpStatus != 200 || reply()->bytesAvailable() == 0) {
        qCWarning(lcCheckServerJob) << "error: status.php replied " << httpStatus;
        emit instanceNotFound(reply());
    } else {
        const QByteArray body = reply()->peek(4 * 1024);
        QJsonParseError error;
        auto status = QJsonDocument::fromJson(body, &error);
        // empty or invalid response
        if (error.error != QJsonParseError::NoError || status.isNull()) {
            qCWarning(lcCheckServerJob) << "status.php from server is not valid JSON!" << body << reply()->request().url() << error.errorString();
        }

        qCInfo(lcCheckServerJob) << "status.php returns: " << status << " " << reply()->error() << " Reply: " << reply();
        if (status.object().contains(QStringLiteral("installed"))) {
            emit instanceFound(_serverUrl, status.object());
        } else {
            qCWarning(lcCheckServerJob) << "No proper answer on " << reply()->url();
            emit instanceNotFound(reply());
        }
    }
    return true;
}

/*********************************************************************************************/


void PropfindJob::start()
{
    connect(this, &LsColJob::directoryListingIterated, this, [this](const QString &, const QMap<QString, QString> &values) {
        OC_ASSERT(!_done);
        _done = true;
        Q_EMIT result(values);
    });
    QNetworkRequest req;
    req.setRawHeader(QByteArrayLiteral("Depth"), QByteArrayLiteral("0"));
    startImpl(req);
}


/*********************************************************************************************/

#ifndef TOKEN_AUTH_ONLY
AvatarJob::AvatarJob(AccountPtr account, const QString &userId, int size, QObject *parent)
    : AbstractNetworkJob(account, QString(), parent)
{
    if (account->serverVersionInt() >= Account::makeServerVersion(10, 0, 0)) {
        _avatarUrl = Utility::concatUrlPath(account->url(), QStringLiteral("remote.php/dav/avatars/%1/%2.png").arg(userId, QString::number(size)));
    } else {
        _avatarUrl = Utility::concatUrlPath(account->url(), QStringLiteral("index.php/avatar/%1/%2").arg(userId, QString::number(size)));
    }
}

void AvatarJob::start()
{
    QNetworkRequest req;
    sendRequest("GET", _avatarUrl, req);
    AbstractNetworkJob::start();
}

QPixmap AvatarJob::makeCircularAvatar(const QPixmap &baseAvatar)
{
    int dim = baseAvatar.width();

    QPixmap avatar(dim, dim);
    avatar.fill(Qt::transparent);

    QPainter painter(&avatar);
    painter.setRenderHint(QPainter::Antialiasing);

    QPainterPath path;
    path.addEllipse(0, 0, dim, dim);
    painter.setClipPath(path);

    painter.drawPixmap(0, 0, baseAvatar);
    painter.end();

    return avatar;
}

bool AvatarJob::finished()
{
    int http_result_code = reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    QPixmap avImage;

    if (http_result_code == 200) {
        QByteArray pngData = reply()->readAll();
        if (pngData.size()) {
            if (avImage.loadFromData(pngData)) {
                qCDebug(lcAvatarJob) << "Retrieved Avatar pixmap!";
            }
        }
    }
    emit avatarPixmap(avImage);
    return true;
}
#endif

/*********************************************************************************************/

ProppatchJob::ProppatchJob(AccountPtr account, const QString &path, QObject *parent)
    : AbstractNetworkJob(account, path, parent)
{
}

void ProppatchJob::start()
{
    if (_properties.isEmpty()) {
        qCWarning(lcProppatchJob) << "Proppatch with no properties!";
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
            keyName = keyName.mid(colIdx + 1);
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
        + propStr + "  </d:prop></d:set>\n"
                    "</d:propertyupdate>\n";

    QBuffer *buf = new QBuffer(this);
    buf->setData(xml);
    buf->open(QIODevice::ReadOnly);
    sendRequest("PROPPATCH", makeDavUrl(path()), req, buf);
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
    qCInfo(lcProppatchJob) << "PROPPATCH of" << reply()->request().url() << "FINISHED WITH STATUS"
                           << replyStatusString();

    int http_result_code = reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (http_result_code == 207) {
        emit success();
    } else {
        qCWarning(lcProppatchJob) << "*not* successful, http result code is" << http_result_code
                                  << (http_result_code == 302 ? reply()->header(QNetworkRequest::LocationHeader).toString() : QLatin1String(""));
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
    sendRequest("HEAD", makeAccountUrl(path()));
    AbstractNetworkJob::start();
}

bool EntityExistsJob::finished()
{
    emit exists(reply());
    return true;
}

/*********************************************************************************************/

JsonApiJob::JsonApiJob(const AccountPtr &account, const QString &path, QObject *parent)
    : AbstractNetworkJob(account, path, parent)
{
}

void JsonApiJob::addQueryParams(const QUrlQuery &params)
{
    _additionalParams = params;
}

void JsonApiJob::start()
{
    startWithRequest(QNetworkRequest());
}

void OCC::JsonApiJob::startWithRequest(QNetworkRequest req)
{
    req.setRawHeader("OCS-APIREQUEST", "true");
    auto query = _additionalParams;
    query.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
    QUrl url = Utility::concatUrlPath(account()->url(), path(), query);
    sendRequest("GET", url, req);
    AbstractNetworkJob::start();
}

bool JsonApiJob::finished()
{
    qCInfo(lcJsonApiJob) << "JsonApiJob of" << reply()->request().url() << "FINISHED WITH STATUS"
                         << replyStatusString();

    int statusCode = 0;

    if (reply()->error() != QNetworkReply::NoError) {
        qCWarning(lcJsonApiJob) << "Network error: " << this << errorString() << reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        emit jsonReceived(QJsonDocument(), statusCode);
        return true;
    }

    QString jsonStr = QString::fromUtf8(reply()->readAll());
    if (jsonStr.contains(QLatin1String("<?xml version=\"1.0\"?>"))) {
        QRegExp rex(QStringLiteral("<statuscode>(\\d+)</statuscode>"));
        if (jsonStr.contains(rex)) {
            // this is a error message coming back from ocs.
            statusCode = rex.cap(1).toInt();
        }

    } else {
        QRegExp rex(QStringLiteral("\"statuscode\":(\\d+),"));
        // example: "{"ocs":{"meta":{"status":"ok","statuscode":100,"message":null},"data":{"version":{"major":8,"minor":"... (504)
        if (jsonStr.contains(rex)) {
            statusCode = rex.cap(1).toInt();
        }
    }

    QJsonParseError error;
    auto json = QJsonDocument::fromJson(jsonStr.toUtf8(), &error);
    // empty or invalid response
    if (error.error != QJsonParseError::NoError || json.isNull()) {
        qCWarning(lcJsonApiJob) << "invalid JSON!" << jsonStr << error.errorString();
        emit jsonReceived(json, statusCode);
        return true;
    }

    emit jsonReceived(json, statusCode);
    return true;
}

DetermineAuthTypeJob::DetermineAuthTypeJob(AccountPtr account, QObject *parent)
    : AbstractNetworkJob(account, QString(), parent)
{
    setAuthenticationJob(true);
    setIgnoreCredentialFailure(true);
}

void DetermineAuthTypeJob::start()
{
    qCInfo(lcDetermineAuthTypeJob) << "Determining auth type for" << _account->davUrl();

    QNetworkRequest req;
    // Prevent HttpCredentialsAccessManager from setting an Authorization header.
    req.setAttribute(HttpCredentials::DontAddCredentialsAttribute, true);
    // Don't reuse previous auth credentials
    req.setAttribute(QNetworkRequest::AuthenticationReuseAttribute, QNetworkRequest::Manual);
    sendRequest("PROPFIND", _account->davUrl(), req);
    AbstractNetworkJob::start();
}

bool DetermineAuthTypeJob::finished()
{
    auto authChallenge = reply()->rawHeader("WWW-Authenticate").toLower();
    auto result = AuthType::Basic;
    if (authChallenge.contains("bearer ")) {
        result = AuthType::OAuth;
    } else if (authChallenge.isEmpty()) {
        qCWarning(lcDetermineAuthTypeJob) << "Did not receive WWW-Authenticate reply to auth-test PROPFIND";
    }
    qCInfo(lcDetermineAuthTypeJob) << "Auth type for" << _account->davUrl() << "is" << result;
    emit this->authType(result);
    return true;
}

SimpleNetworkJob::SimpleNetworkJob(AccountPtr account, QObject *parent)
    : AbstractNetworkJob(account, QString(), parent)
{
}

void SimpleNetworkJob::start()
{
    sendRequest(_simpleVerb, _simpleUrl, _simpleRequest, _simpleBody);
    AbstractNetworkJob::start();
}

void SimpleNetworkJob::addNewReplyHook(std::function<void(QNetworkReply *)> &&hook)
{
    _replyHooks.push_back(hook);
}

void SimpleNetworkJob::prepareRequest(const QByteArray &verb, const QUrl &url,
    const QNetworkRequest &req, QIODevice *requestBody)
{
    _simpleVerb = verb;
    _simpleUrl = url;
    _simpleRequest = req;
    _simpleBody = requestBody;
}

void SimpleNetworkJob::prepareRequest(const QByteArray &verb, const QUrl &url, const QNetworkRequest &req, const QUrlQuery &arguments)
{
    // not a leak
    auto requestBody = new QBuffer {};
    requestBody->setData(arguments.query(QUrl::FullyEncoded).toUtf8());
    auto newReq = req;
    newReq.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded; charset=UTF-8"));
    return prepareRequest(verb, url, newReq, requestBody);
}

void SimpleNetworkJob::prepareRequest(const QByteArray &verb, const QUrl &url, const QNetworkRequest &req, const QJsonObject &arguments)
{
    // not a leak
    auto requestBody = new QBuffer {};
    requestBody->setData(QJsonDocument(arguments).toJson());
    auto newReq = req;
    newReq.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    return prepareRequest(verb, url, newReq, requestBody);
}

bool SimpleNetworkJob::finished()
{
    emit finishedSignal(reply());
    return true;
}

void SimpleNetworkJob::newReplyHook(QNetworkReply *reply)
{
    for (const auto &hook : _replyHooks) {
        hook(reply);
    }
}

void fetchPrivateLinkUrl(AccountPtr account, const QString &remotePath, QObject *target,
    std::function<void(const QString &url)> targetFun)
{
    // Retrieve the new link by PROPFIND
    PropfindJob *job = new PropfindJob(account, remotePath, target);
    job->setProperties({ QByteArrayLiteral("http://owncloud.org/ns:privatelink") });
    job->setTimeout(10 * 1000);
    QObject::connect(job, &PropfindJob::result, target, [=](const QMap<QString, QString> &result) {
        auto privateLinkUrl = result[QStringLiteral("privatelink")];
        if (!privateLinkUrl.isEmpty()) {
            targetFun(privateLinkUrl);
        }
    });
    job->start();
}

} // namespace OCC
