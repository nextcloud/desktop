/*
 * SPDX-FileCopyrightText: 2017 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2013 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "networkjobs.h"
#include "account.h"
#include "helpers.h"
#include "owncloudpropagator.h"
#include "clientsideencryption.h"
#include "common/checksums.h"

#include "creds/abstractcredentials.h"
#include "creds/httpcredentials.h"
#include "configfile.h"

#include <QJsonDocument>
#include <QLoggingCategory>
#include <QNetworkRequest>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslConfiguration>
#include <QSslCipher>
#include <QBuffer>
#include <QXmlStreamReader>
#include <QDomDocument>
#include <QStringList>
#include <QStack>
#include <QTimer>
#include <QMutex>
#include <QCoreApplication>
#include <QJsonObject>
#include <QLoggingCategory>
#ifndef TOKEN_AUTH_ONLY
#include <QPainter>
#include <QPainterPath>
#endif

using namespace Qt::StringLiterals;

namespace OCC {

Q_LOGGING_CATEGORY(lcEtagJob, "nextcloud.sync.networkjob.etag", QtInfoMsg)
Q_LOGGING_CATEGORY(lcLsColJob, "nextcloud.sync.networkjob.lscol", QtInfoMsg)
Q_LOGGING_CATEGORY(lcCheckServerJob, "nextcloud.sync.networkjob.checkserver", QtInfoMsg)
Q_LOGGING_CATEGORY(lcCheckRedirectCostFreeUrlJob, "nextcloud.sync.networkjob.checkredirectcostfreeurl", QtInfoMsg)
Q_LOGGING_CATEGORY(lcPropfindJob, "nextcloud.sync.networkjob.propfind", QtInfoMsg)
Q_LOGGING_CATEGORY(lcAvatarJob, "nextcloud.sync.networkjob.avatar", QtInfoMsg)
Q_LOGGING_CATEGORY(lcMkColJob, "nextcloud.sync.networkjob.mkcol", QtInfoMsg)
Q_LOGGING_CATEGORY(lcProppatchJob, "nextcloud.sync.networkjob.proppatch", QtInfoMsg)
Q_LOGGING_CATEGORY(lcJsonApiJob, "nextcloud.sync.networkjob.jsonapi", QtInfoMsg)
Q_LOGGING_CATEGORY(lcSimpleApiJob, "nextcloud.sync.networkjob.simpleapi", QtInfoMsg)
Q_LOGGING_CATEGORY(lcDetermineAuthTypeJob, "nextcloud.sync.networkjob.determineauthtype", QtInfoMsg)
Q_LOGGING_CATEGORY(lcSimpleFileJob, "nextcloud.sync.networkjob.simplefilejob", QtInfoMsg)

constexpr auto notModifiedStatusCode = 304;
constexpr auto propfindPropElementTagName = "prop";
constexpr auto propfindFileTagElementTagName = "tag";
constexpr auto propfindFileTagsContainerElementTagName = "tags";
constexpr auto propfindSystemFileTagElementTagName = "system-tag";
constexpr auto propfindSystemFileTagsContainerElementTagName = "system-tags";

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
    auto *buf = new QBuffer(this);
    buf->setData(xml);
    buf->open(QIODevice::ReadOnly);
    // assumes ownership
    sendRequest("PROPFIND", makeDavUrl(path()), req, buf);

    if (reply()->error() != QNetworkReply::NoError) {
        qCWarning(lcEtagJob) << "request network error: " << reply()->errorString();
    }
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
        emit etagRetrieved(etag, QDateTime::fromString(QString::fromUtf8(_responseTimestamp), Qt::RFC2822Date));
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

MkColJob::MkColJob(AccountPtr account, const QString &path, const QMap<QByteArray, QByteArray> &extraHeaders, QObject *parent)
    : AbstractNetworkJob(account, path, parent)
    , _extraHeaders(extraHeaders)
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


LsColXMLParser::LsColXMLParser() = default;

bool LsColXMLParser::parse(const QByteArray &xml, QHash<QString, ExtraFolderInfo> *fileInfo, const QString &expectedPath)
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
                QString hrefString = QUrl::fromLocalFile(QUrl::fromPercentEncoding(reader.readElementText().toUtf8()))
                        .adjusted(QUrl::NormalizePathSegments)
                        .path();
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
                if (ok && fileInfo) {
                    (*fileInfo)[currentHref].size = s;
                }
            } else if (name == QLatin1String("fileid")) {
                (*fileInfo)[currentHref].fileId = propertyContent.toUtf8();
            }
            currentTmpProperties.insert(reader.name().toString(), propertyContent);
        }

        // End elements with DAV:
        if (type == QXmlStreamReader::EndElement) {
            if (reader.namespaceUri() == QLatin1String("DAV:")) {
                if (reader.name() == QStringLiteral("response")) {
                    if (currentHref.endsWith('/')) {
                        currentHref.chop(1);
                    }
                    emit directoryListingIterated(currentHref, currentHttp200Properties);
                    currentHref.clear();
                    currentHttp200Properties.clear();
                } else if (reader.name() == QStringLiteral("propstat")) {
                    insidePropstat = false;
                    if (currentPropsHaveHttp200) {
                        currentHttp200Properties = QMap<QString, QString>(currentTmpProperties);
                    }
                    currentTmpProperties.clear();
                    currentPropsHaveHttp200 = false;
                } else if (reader.name() == QStringLiteral("prop")) {
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

LsColJob::LsColJob(AccountPtr account, const QString &path)
    : AbstractNetworkJob(account, path)
{
}

LsColJob::LsColJob(AccountPtr account, const QUrl &url)
    : AbstractNetworkJob(account, QString())
    , _url(url)
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

QList<QByteArray> LsColJob::defaultProperties(FolderType isRootPath, AccountPtr account)
{
    auto props = QList<QByteArray>{};

    props << "resourcetype"
          << "getlastmodified"
          << "getcontentlength"
          << "getetag"
          << "quota-available-bytes"
          << "quota-used-bytes"
          << "http://owncloud.org/ns:size"
          << "http://owncloud.org/ns:id"
          << "http://owncloud.org/ns:fileid"
          << "http://owncloud.org/ns:downloadURL"
          << "http://owncloud.org/ns:dDC"
          << "http://owncloud.org/ns:permissions"
          << "http://owncloud.org/ns:checksums"
          << "http://nextcloud.org/ns:is-encrypted"
          << "http://nextcloud.org/ns:metadata-files-live-photo"
          << "http://nextcloud.org/ns:share-attributes";

    if (isRootPath == FolderType::RootFolder) {
        props << "http://owncloud.org/ns:data-fingerprint";
    }

    if (account->serverVersionInt() >= Account::makeServerVersion(10, 0, 0)) {
        // Server older than 10.0 have performances issue if we ask for the share-types on every PROPFIND
        props << "http://owncloud.org/ns:share-types";
    }
    if (account->capabilities().filesLockAvailable()) {
        props << "http://nextcloud.org/ns:lock"
              << "http://nextcloud.org/ns:lock-owner-displayname"
              << "http://nextcloud.org/ns:lock-owner"
              << "http://nextcloud.org/ns:lock-owner-type"
              << "http://nextcloud.org/ns:lock-owner-editor"
              << "http://nextcloud.org/ns:lock-time"
              << "http://nextcloud.org/ns:lock-timeout"
              << "http://nextcloud.org/ns:lock-token";
    }
    props << "http://nextcloud.org/ns:is-mount-root"
          << "http://nextcloud.org/ns:request-upload";

    return props;
}

void LsColJob::propertyMapToRemoteInfo(const QMap<QString, QString> &map, RemotePermissions::MountedPermissionAlgorithm algorithm, RemoteInfo &result)
{
    for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
        QString property = it.key();
        QString value = it.value();
        if (property == "resourcetype"_L1) {
            result.isDirectory = value.contains("collection"_L1);
        } else if (property == "getlastmodified"_L1) {
            value.replace("GMT", "+0000");
            const auto date = QDateTime::fromString(value, Qt::RFC2822Date);
            Q_ASSERT(date.isValid());
            result.modtime = 0;
            if (date.toSecsSinceEpoch() > 0) {
                result.modtime = date.toSecsSinceEpoch();
            }
        } else if (property == "getcontentlength"_L1) {
            // See #4573, sometimes negative size values are returned
            bool ok = false;
            qlonglong ll = value.toLongLong(&ok);
            if (ok && ll >= 0) {
                result.size = ll;
            } else {
                result.size = 0;
            }
        } else if (property == "getetag"_L1) {
            result.etag = Utility::normalizeEtag(value.toUtf8());
        } else if (property == "id"_L1) {
            result.fileId = value.toUtf8();
        } else if (property == "downloadURL"_L1) {
            result.directDownloadUrl = value;
        } else if (property == "dDC"_L1) {
            result.directDownloadCookies = value;
        } else if (property == "permissions"_L1) {
            result.remotePerm = RemotePermissions::fromServerString(value, algorithm, map);
        } else if (property == "checksums"_L1) {
            result.checksumHeader = findBestChecksum(value.toUtf8());
        } else if (property == "share-types"_L1 && !value.isEmpty()) {
            // Since QMap is sorted, "share-types" is always after "permissions".
            if (result.remotePerm.isNull()) {
                qWarning() << "Server returned a share type, but no permissions?";
            } else {
                // S means shared with me.
                // But for our purpose, we want to know if the file is shared. It does not matter
                // if we are the owner or not.
                // Piggy back on the permission field
                result.remotePerm.setPermission(RemotePermissions::IsShared);
                result.sharedByMe = true;
            }
        } else if (property == "is-encrypted"_L1 && value == "1"_L1) {
            result._isE2eEncrypted = true;
        } else if (property == "lock"_L1) {
            result.locked = (value == "1"_L1 ? SyncFileItem::LockStatus::LockedItem : SyncFileItem::LockStatus::UnlockedItem);
        }
        if (property == "lock-owner-displayname"_L1) {
            result.lockOwnerDisplayName = value;
        }
        if (property == "lock-owner"_L1) {
            result.lockOwnerId = value;
        }
        if (property == "lock-owner-type"_L1) {
            auto ok = false;
            const auto intConvertedValue = value.toULongLong(&ok);
            if (ok) {
                result.lockOwnerType = static_cast<SyncFileItem::LockOwnerType>(intConvertedValue);
            } else {
                result.lockOwnerType = SyncFileItem::LockOwnerType::UserLock;
            }
        }
        if (property == "lock-owner-editor"_L1) {
            result.lockEditorApp = value;
        }
        if (property == "lock-time"_L1) {
            auto ok = false;
            const auto intConvertedValue = value.toULongLong(&ok);
            if (ok) {
                result.lockTime = intConvertedValue;
            } else {
                result.lockTime = 0;
            }
        }
        if (property == "lock-timeout"_L1) {
            auto ok = false;
            const auto intConvertedValue = value.toULongLong(&ok);
            if (ok) {
                result.lockTimeout = intConvertedValue;
            } else {
                result.lockTimeout = 0;
            }
        }
        if (property == "lock-token"_L1) {
            result.lockToken = value;
        }
        if (property == "metadata-files-live-photo"_L1) {
            result.livePhotoFile = value;
            result.isLivePhoto = true;
        }
        if (property == "request-upload"_L1) {
            result.requestUpload = (value == "1"_L1);
        }
    }

    if (result.isDirectory && map.contains("size"_L1)) {
        result.sizeOfFolder = map.value("size"_L1).toInt();
    }

    if (result.isDirectory && map.contains(FolderQuota::usedBytesC) && map.contains(FolderQuota::availableBytesC)) {
        // The server can respond with e.g. "2.58440798353E+12" for the quota
        // therefore: parse the string as a double and cast it to i64
        auto ok = false;
        auto quotaValue = static_cast<int64_t>(map.value(FolderQuota::usedBytesC).toDouble(&ok));
        result.folderQuota.bytesUsed = ok ? quotaValue : -1;
        quotaValue = static_cast<int64_t>(map.value(FolderQuota::availableBytesC).toDouble(&ok));
        result.folderQuota.bytesAvailable = ok ? quotaValue : -1;
    }
}

void LsColJob::start()
{
    QList<QByteArray> properties = _properties;

    if (properties.isEmpty()) {
        qCWarning(lcLsColJob) << "Propfind with no properties!";
    }
    QByteArray propStr;
    for (const auto &prop : properties) {
        if (prop.contains(':')) {
            int colIdx = prop.lastIndexOf(":");
            auto ns = prop.left(colIdx);
            if (ns == "http://owncloud.org/ns") {
                propStr += "    <oc:" + prop.mid(colIdx + 1) + " />\n";
            } else {
                propStr += "    <" + prop.mid(colIdx + 1) + " xmlns=\"" + ns + "\" />\n";
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
        + propStr + "  </d:prop>\n"
                    "</d:propfind>\n");
    auto *buf = new QBuffer(this);
    buf->setData(xml);
    buf->open(QIODevice::ReadOnly);
    if (_url.isValid()) {
        sendRequest("PROPFIND", _url, req, buf);
    } else {
        sendRequest("PROPFIND", makeDavUrl(path()), req, buf);
    }
    AbstractNetworkJob::start();
}

// TODO: Instead of doing all in this slot, we should iteratively parse in readyRead(). This
// would allow us to be more asynchronous in processing while data is coming from the network,
// not all in one big blob at the end.
bool LsColJob::finished()
{
    qCInfo(lcLsColJob) << "LSCOL of" << reply()->request().url() << "FINISHED WITH STATUS"
                       << replyStatusString();

    const auto contentType = reply()->header(QNetworkRequest::ContentTypeHeader).toString();
    const auto httpCode = reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const auto validContentType = contentType.contains("application/xml; charset=utf-8") ||
                                  contentType.contains("application/xml; charset=\"utf-8\"") ||
                                  contentType.contains("text/xml; charset=utf-8") ||
                                  contentType.contains("text/xml; charset=\"utf-8\"");

    if (httpCode == 207 && validContentType) {
        LsColXMLParser parser;
        connect(&parser, &LsColXMLParser::directoryListingSubfolders,
            this, &LsColJob::directoryListingSubfolders);
        connect(&parser, &LsColXMLParser::directoryListingIterated,
            this, &LsColJob::directoryListingIterated);
        connect(&parser, &LsColXMLParser::finishedWithError,
            this, &LsColJob::finishedWithError);
        connect(&parser, &LsColXMLParser::finishedWithoutError,
            this, &LsColJob::finishedWithoutError);

        // bool LsColXMLParser::parse takes a while, let's process some events in attempt to make UI more responsive
        // from https://doc.qt.io/qt-5/qcoreapplication.html#processEvents-1 
        // "You can call this function occasionally when your program is busy doing a long operation (e.g. copying a file)."
        // we should not abuse this function, as it affects QObject instances lifetime (when children are getting deleted or when deleteLater is called)
        // one reason I had to remove ability for LsColJob to have parent, which, otherwise, leads to a crash later
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);

        QString expectedPath = reply()->request().url().path(); // something like "/owncloud/remote.php/dav/folder"
        if (!parser.parse(reply()->readAll(), &_folderInfos, expectedPath)) {
            // XML parse error
            emit finishedWithError(reply());
        }
    } else {
        // wrong content type, wrong HTTP code or any other network error
        emit finishedWithError(reply());
    }

    this->deleteLater();

    // fix crash on random deletion mess in the parent class, we never discard this job but always delete it inside this method
    return false;
}

/*********************************************************************************************/

namespace {
    const char statusphpC[] = "status.php";
    const char nextcloudDirC[] = "nextcloud/";
}

CheckServerJob::CheckServerJob(AccountPtr account, QObject *parent)
    : AbstractNetworkJob(account, QLatin1String(statusphpC), parent)
{
    setIgnoreCredentialFailure(true);
    connect(this, &AbstractNetworkJob::redirected,
        this, &CheckServerJob::slotRedirected);
}

void CheckServerJob::start()
{
    _serverUrl = account()->url();
    sendRequest("GET", Utility::concatUrlPath(_serverUrl, path()));
    connect(reply(), &QNetworkReply::metaDataChanged, this, &CheckServerJob::metaDataChangedSlot);
    connect(reply(), &QNetworkReply::encrypted, this, &CheckServerJob::encryptedSlot);
    AbstractNetworkJob::start();
}

void CheckServerJob::onTimedOut()
{
    qCWarning(lcCheckServerJob) << "TIMEOUT";
    if (reply() && reply()->isRunning()) {
        emit timeout(reply()->url());
    } else if (!reply()) {
        qCWarning(lcCheckServerJob) << "Timeout even there was no reply?";
    }
    deleteLater();
}

QString CheckServerJob::version(const QJsonObject &info)
{
    return info.value(QLatin1String("version")).toString();
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

void CheckServerJob::slotRedirected(QNetworkReply *reply, const QUrl &targetUrl, int redirectCount)
{
    QByteArray slashStatusPhp("/");
    slashStatusPhp.append(statusphpC);

    int httpCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QString path = targetUrl.path();
    if ((httpCode == 301 || httpCode == 308) // permanent redirection
        && redirectCount == _permanentRedirects // don't apply permanent redirects after a temporary one
        && path.endsWith(slashStatusPhp)) {
        _serverUrl = targetUrl;
        _serverUrl.setPath(path.left(path.size() - slashStatusPhp.size()));
        qCInfo(lcCheckServerJob) << "status.php was permanently redirected to"
                                 << targetUrl << "new server url is" << _serverUrl;
        ++_permanentRedirects;
    }
}

void CheckServerJob::metaDataChangedSlot()
{
    account()->setSslConfiguration(reply()->sslConfiguration());
    mergeSslConfigurationForSslButton(reply()->sslConfiguration(), account());
}


bool CheckServerJob::finished()
{
    if (reply()->request().url().scheme() == QLatin1String("https")
        && reply()->sslConfiguration().sessionTicket().isEmpty()
        && reply()->error() == QNetworkReply::NoError) {
        qCWarning(lcCheckServerJob) << "No SSL session identifier / session ticket is used, this might impact sync performance negatively.";
    }

    mergeSslConfigurationForSslButton(reply()->sslConfiguration(), account());

    // The server installs to /owncloud. Let's try that if the file wasn't found
    // at the original location
    if ((reply()->error() == QNetworkReply::ContentNotFoundError) && (!_subdirFallback)) {
        _subdirFallback = true;
        setPath(QLatin1String(nextcloudDirC) + QLatin1String(statusphpC));
        start();
        qCInfo(lcCheckServerJob) << "Retrying with" << reply()->url();
        return false;
    }

    QByteArray body = reply()->peek(4 * 1024);
    int httpStatus = reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (body.isEmpty() || httpStatus != 200) {
        qCWarning(lcCheckServerJob) << "error: status.php replied " << httpStatus << body;
        emit instanceNotFound(reply());
    } else {
        QJsonParseError error{};
        auto status = QJsonDocument::fromJson(body, &error);
        // empty or invalid response
        if (error.error != QJsonParseError::NoError || status.isNull()) {
            qCWarning(lcCheckServerJob) << "status.php from server is not valid JSON!" << body << reply()->request().url() << error.errorString();
        }

        qCInfo(lcCheckServerJob) << "status.php returns: " << status << " " << reply()->error() << " Reply: " << reply();
        if (status.object().contains("installed")) {
            emit instanceFound(_serverUrl, status.object());
        } else {
            qCWarning(lcCheckServerJob) << "No proper answer on " << reply()->url();
            emit instanceNotFound(reply());
        }
    }
    return true;
}

/*********************************************************************************************/

CheckRedirectCostFreeUrlJob::CheckRedirectCostFreeUrlJob(const AccountPtr &account, QObject *parent)
    : AbstractNetworkJob(account, QLatin1String(statusphpC), parent)
{
    setIgnoreCredentialFailure(true);
}

void CheckRedirectCostFreeUrlJob::start()
{
    setFollowRedirects(false);
    sendRequest("GET", Utility::concatUrlPath(account()->url(), QStringLiteral("/index.php/204")));
    AbstractNetworkJob::start();
}

void CheckRedirectCostFreeUrlJob::onTimedOut()
{
    qCDebug(lcCheckRedirectCostFreeUrlJob) << "TIMEOUT";
    if (reply() && reply()->isRunning()) {
        emit timeout(reply()->url());
    } else if (!reply()) {
        qCDebug(lcCheckRedirectCostFreeUrlJob) << "Timeout without a reply?";
    }
    AbstractNetworkJob::onTimedOut();
}

bool CheckRedirectCostFreeUrlJob::finished()
{
    const auto statusCode = reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (statusCode >= 301 && statusCode <= 307) {
        const auto redirectionTarget = reply()->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
        qCDebug(lcCheckRedirectCostFreeUrlJob) << "Redirecting cost-free URL" << reply()->url() << " to" << redirectionTarget;
    }
    emit jobFinished(statusCode);
    return true;
}
/*********************************************************************************************/

PropfindJob::PropfindJob(AccountPtr account, const QString &path, QObject *parent)
    : AbstractNetworkJob(account, path, parent)
{
}

void PropfindJob::start()
{
    const auto properties = _properties;

    if (properties.isEmpty()) {
        qCWarning(lcLsColJob) << "Propfind with no properties!";
    }
    QNetworkRequest req;
    // Always have a higher priority than the propagator because we use this from the UI
    // and really want this to be done first (no matter what internal scheduling QNAM uses).
    // Also possibly useful for avoiding false timeouts.
    req.setPriority(QNetworkRequest::HighPriority);
    req.setRawHeader("Depth", "0");
    QByteArray propStr;
    for (const auto &prop : properties) {
        if (prop.contains(':')) {
            const auto colIdx = prop.lastIndexOf(":");
            propStr += "    <" + prop.mid(colIdx + 1) + " xmlns=\"" + prop.left(colIdx) + "\" />\n";
        } else {
            propStr += "    <d:" + prop + " />\n";
        }
    }
    const auto xml = QByteArray("<?xml version=\"1.0\" ?>\n"
                                "<d:propfind xmlns:d=\"DAV:\">\n"
                                "  <d:prop>\n"
                    + propStr + "  </d:prop>\n"
                                "</d:propfind>\n");

    const auto buf = new QBuffer(this);
    buf->setData(xml);
    buf->open(QIODevice::ReadOnly);
    sendRequest("PROPFIND", makeDavUrl(path()), req, buf);

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
    qCInfo(lcPropfindJob) << "PROPFIND of" << reply()->request().url() << "FINISHED WITH STATUS"
                          << replyStatusString();

    const auto http_result_code = reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (http_result_code == 207) {
        // Parse DAV response
        auto domDocument = QDomDocument();

        if (const auto res = domDocument.setContent(reply(), QDomDocument::ParseOption::UseNamespaceProcessing); !res) {
            qCWarning(lcPropfindJob) << "XML parser error: " << res.errorMessage << res.errorLine << res.errorColumn;
            emit finishedWithError(reply());

        } else {
            const auto parsedItems = processPropfindDomDocument(domDocument);
            emit result(parsedItems);
        }

    } else {
        qCWarning(lcPropfindJob) << "*not* successful, http result code is" << http_result_code
                                 << (http_result_code == 302 ? reply()->header(QNetworkRequest::LocationHeader).toString() : QLatin1String(""));
        emit finishedWithError(reply());
    }

    return true;
}

QVariantMap PropfindJob::processPropfindDomDocument(const QDomDocument &domDocument)
{
    if (!domDocument.hasChildNodes()) {
        return {};
    }

    auto items = QVariantMap();

    const auto rootElement = domDocument.documentElement();
    const auto propNodes = rootElement.elementsByTagName(propfindPropElementTagName);

    for (auto i = 0; i < propNodes.count(); ++i) {
        const auto propNode = propNodes.at(i);
        const auto propElement = propNode.toElement();

        if (propElement.isNull() || propElement.tagName() != propfindPropElementTagName) {
            continue;
        }

        auto propChildNode = propElement.firstChild();

        while (!propChildNode.isNull()) {
            const auto propChildElement = propChildNode.toElement();

            if (!propChildElement.isNull()) {
                const auto propChildElementTagName = propChildElement.tagName();

                if (propChildElementTagName == propfindFileTagsContainerElementTagName) {
                    const auto tagList = processTagsInPropfindDomDocument(domDocument);
                    items.insert(propChildElementTagName, tagList);
                } else if (propChildElementTagName == propfindSystemFileTagsContainerElementTagName) {
                    const auto systemTagList = processSystemTagsInPropfindDomDocument(domDocument);
                    items.insert(propChildElementTagName, systemTagList);
                } else {
                    items.insert(propChildElementTagName, propChildElement.text());
                }
            }

            propChildNode = propChildNode.nextSibling();
        }
    }

    return items;
}

QStringList PropfindJob::processTagsInPropfindDomDocument(const QDomDocument &domDocument)
{
    const auto tagNodes = domDocument.elementsByTagName(propfindFileTagElementTagName);
    if (tagNodes.isEmpty()) {
        return {};
    }

    const auto tagCount = tagNodes.count();
    auto tagList = QStringList();
    tagList.reserve(tagCount);

    for (auto i = 0; i < tagCount; ++i) {
        const auto tagNode = tagNodes.at(i);
        const auto tagElement = tagNode.toElement();

        if (tagElement.isNull()) {
            continue;
        }

        tagList.append(tagElement.text());
    }

    return tagList;
}

QVariantList PropfindJob::processSystemTagsInPropfindDomDocument(const QDomDocument &domDocument)
{
    const auto systemTagNodes = domDocument.elementsByTagName(propfindSystemFileTagElementTagName);
    if (systemTagNodes.isEmpty()) {
        return {};
    }

    const auto systemTagCount = systemTagNodes.count();
    auto systemTagsList = QVariantList();

    for (auto i = 0; i < systemTagCount; ++i) {
        const auto systemTagNode = systemTagNodes.at(i);
        const auto systemTagElem = systemTagNode.toElement();

        if (systemTagElem.isNull()) {
            continue;
        }

        auto systemTagMap = QVariantMap{ { QStringLiteral("tag"), systemTagElem.text() } };

        const auto systemTagElemAttrs = systemTagElem.attributes();
        for (auto i = 0; i < systemTagElemAttrs.count(); ++i) {
            const auto systemTagElemAttrNode = systemTagElemAttrs.item(i);
            const auto systemTagElemAttr = systemTagElemAttrNode.toAttr();

            systemTagMap.insert(systemTagElemAttr.name(), systemTagElemAttr.value());
        }

        systemTagsList.append(systemTagMap);
    }

    return systemTagsList;
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

QImage AvatarJob::makeCircularAvatar(const QImage &baseAvatar)
{
    if (baseAvatar.isNull()) {
        return {};
    }

    int dim = baseAvatar.width();

    QImage avatar(dim, dim, QImage::Format_ARGB32);
    avatar.fill(Qt::transparent);

    QPainter painter(&avatar);
    painter.setRenderHint(QPainter::Antialiasing);

    QPainterPath path;
    path.addEllipse(0, 0, dim, dim);
    painter.setClipPath(path);

    painter.drawImage(0, 0, baseAvatar);
    painter.end();

    return avatar;
}

bool AvatarJob::finished()
{
    int http_result_code = reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    QImage avImage;

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

    auto *buf = new QBuffer(this);
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
    : SimpleApiJob(account, path, parent)
{
}

void JsonApiJob::setBody(const QJsonDocument &body)
{
    SimpleApiJob::setBody(body.toJson());
    qCDebug(lcJsonApiJob) << "Set body for request:" << SimpleApiJob::body();
    if (!SimpleApiJob::body().isEmpty()) {
        request().setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    }
}

void JsonApiJob::start()
{
    additionalParams().addQueryItem(QLatin1String("format"), QLatin1String("json"));
    SimpleApiJob::start();
}

bool JsonApiJob::finished()
{
    qCInfo(lcJsonApiJob) << "JsonApiJob of" << reply()->request().url() << "FINISHED WITH STATUS"
                         << replyStatusString();

    int statusCode = 0;
    int httpStatusCode = reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply()->error() != QNetworkReply::NoError) {
        qCWarning(lcJsonApiJob) << "Network error: " << path() << errorString() << reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        statusCode = reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        emit jsonReceived(QJsonDocument(), statusCode);
        return true;
    }

    QString jsonStr = QString::fromUtf8(reply()->readAll());
    if (jsonStr.contains("<?xml version=\"1.0\"?>")) {
        static const QRegularExpression rex("<statuscode>(\\d+)</statuscode>");
        const auto rexMatch = rex.match(jsonStr);
        if (rexMatch.hasMatch()) {
            // this is a error message coming back from ocs.
            statusCode = rexMatch.captured(1).toInt();
        }
    } else if(jsonStr.isEmpty() && httpStatusCode == notModifiedStatusCode){
        qCWarning(lcJsonApiJob) << "Nothing changed so nothing to retrieve - status code: " << httpStatusCode;
        statusCode = httpStatusCode;
    } else {
        static const QRegularExpression rex(R"("statuscode":(\d+))");
        // example: "{"ocs":{"meta":{"status":"ok","statuscode":100,"message":null},"data":{"version":{"major":8,"minor":"... (504)
        const auto rxMatch = rex.match(jsonStr);
        if (rxMatch.hasMatch()) {
            statusCode = rxMatch.captured(1).toInt();
        }
    }

    // save new ETag value
    if (const auto etagHeader = reply()->header(QNetworkRequest::ETagHeader); etagHeader.isValid()) {
        emit etagResponseHeaderReceived(etagHeader.toByteArray(), statusCode);
    }

    QJsonParseError error{};
    auto json = QJsonDocument::fromJson(jsonStr.toUtf8(), &error);
    // empty or invalid response and status code is != 304 because jsonStr is expected to be empty
    if ((error.error != QJsonParseError::NoError || json.isNull()) && httpStatusCode != notModifiedStatusCode) {
        qCWarning(lcJsonApiJob) << "invalid JSON!" << jsonStr << error.errorString();
        emit jsonReceived(json, statusCode);
        return true;
    }

    emit jsonReceived(json, statusCode);
    return true;
}


DetermineAuthTypeJob::DetermineAuthTypeJob(AccountPtr account, QObject *parent)
    : QObject(parent)
    , _account(account)
{
    useFlow2 = ConfigFile().forceLoginV2();
}

void DetermineAuthTypeJob::start()
{
    qCInfo(lcDetermineAuthTypeJob) << "Determining auth type for" << _account->davUrl();

    QNetworkRequest req;
    // Prevent HttpCredentialsAccessManager from setting an Authorization header.
    req.setAttribute(AbstractCredentials::DontAddCredentialsAttribute, true);
    // Don't reuse previous auth credentials
    req.setAttribute(QNetworkRequest::AuthenticationReuseAttribute, QNetworkRequest::Manual);

    // Start three parallel requests

    // 1. determines whether it's a basic auth server
    auto get = _account->sendRequest("GET", _account->url(), req);

    // 2. checks the HTTP auth method.
    auto propfind = _account->sendRequest("PROPFIND", _account->davUrl(), req);

    // 3. Determines if the old flow has to be used (GS for now)
    auto oldFlowRequired = new JsonApiJob(_account, "/ocs/v2.php/cloud/capabilities", this);

    get->setTimeout(30 * 1000);
    propfind->setTimeout(30 * 1000);
    oldFlowRequired->setTimeout(30 * 1000);
    get->setIgnoreCredentialFailure(true);
    propfind->setIgnoreCredentialFailure(true);
    oldFlowRequired->setIgnoreCredentialFailure(true);

    connect(get, &SimpleNetworkJob::finishedSignal, this, [this, get]() {
        const auto reply = get->reply();
        const auto wwwAuthenticateHeader = reply->rawHeader("WWW-Authenticate");
        if (reply->error() == QNetworkReply::AuthenticationRequiredError
            && (wwwAuthenticateHeader.startsWith("Basic") || wwwAuthenticateHeader.startsWith("Bearer"))) {
            _resultGet = Basic;
        } else {
            _resultGet = LoginFlowV2;
        }
        if (_account->isPublicShareLink()) {
            _resultGet = Basic;
        }
        _getDone = true;
        checkAllDone();
    });
    connect(propfind, &SimpleNetworkJob::finishedSignal, this, [this](QNetworkReply *reply) {
        auto authChallenge = reply->rawHeader("WWW-Authenticate").toLower();

        if (authChallenge.isEmpty()) {
            qCWarning(lcDetermineAuthTypeJob) << "Did not receive WWW-Authenticate reply to auth-test PROPFIND";
        } else {
            qCWarning(lcDetermineAuthTypeJob) << "Unknown WWW-Authenticate reply to auth-test PROPFIND:" << authChallenge;
        }
        _resultPropfind = Basic;
        _propfindDone = true;
        checkAllDone();
    });
    connect(oldFlowRequired, &JsonApiJob::jsonReceived, this, [this](const QJsonDocument &json, int statusCode) {
        if (statusCode == 200) {
            _resultOldFlow = LoginFlowV2;

            auto data = json.object().value("ocs").toObject().value("data").toObject().value("capabilities").toObject();
            auto gs = data.value("globalscale");
            if (gs != QJsonValue::Undefined) {
                auto flow = gs.toObject().value("desktoplogin");
                if (flow != QJsonValue::Undefined) {
                    if (flow.toInt() == 1) {
#ifdef WITH_WEBENGINE
                        if(!this->useFlow2) {
                            _resultOldFlow = WebViewFlow;
                        } else {
                            qCWarning(lcDetermineAuthTypeJob) << "Server only supports flow1, but this client was configured to only use flow2";
                        }
#else // WITH_WEBENGINE
                        qCWarning(lcDetermineAuthTypeJob) << "Server does only support flow1, but this client was compiled without support for flow1";
#endif // WITH_WEBENGINE
                    }
                }
            }
        } else {
            _resultOldFlow = Basic;
        }
        if (_account->isPublicShareLink()) {
            _resultOldFlow = Basic;
        }
        _oldFlowDone = true;
        checkAllDone();
    });

    oldFlowRequired->start();
}

void DetermineAuthTypeJob::checkAllDone()
{
    // Do not conitunue until eve
    if (!_getDone || !_propfindDone || !_oldFlowDone) {
        return;
    }

    Q_ASSERT(_resultGet != NoAuthType);
    Q_ASSERT(_resultPropfind != NoAuthType);
    Q_ASSERT(_resultOldFlow != NoAuthType);

    auto result = _resultPropfind;

#ifdef WITH_WEBENGINE
    // WebViewFlow > Basic
    if (_account->serverVersionInt() >= Account::makeServerVersion(12, 0, 0)) {
        result = WebViewFlow;
        if (useFlow2) {
            result = LoginFlowV2;
        }
    }
#endif // WITH_WEBENGINE

    // LoginFlowV2 > WebViewFlow > Basic
    if (_account->serverVersionInt() >= Account::makeServerVersion(16, 0, 0)) {
        result = LoginFlowV2;
    }

#ifdef WITH_WEBENGINE
    // If we determined that we need the webview flow (GS for example) then we switch to that
    if (_resultOldFlow == WebViewFlow) {
        result = WebViewFlow;
        if (useFlow2) {
            result = LoginFlowV2;
        }
    }
#endif // WITH_WEBENGINE

    // If we determined that a simple get gave us an authentication required error
    // then the server enforces basic auth and we got no choice but to use this
    if (_resultGet == Basic) {
        result = Basic;
    }

    qCInfo(lcDetermineAuthTypeJob) << "Auth type for" << _account->davUrl() << "is" << result;
    emit authType(result);
    deleteLater();
}

SimpleNetworkJob::SimpleNetworkJob(AccountPtr account, QObject *parent)
    : AbstractNetworkJob(account, QString(), parent)
{
}

QNetworkReply *SimpleNetworkJob::startRequest(const QByteArray &verb, const QUrl &url,
    QNetworkRequest req, QIODevice *requestBody)
{
    auto reply = sendRequest(verb, url, req, requestBody);
    start();
    return reply;
}

bool SimpleNetworkJob::finished()
{
    emit finishedSignal(reply());
    return true;
}

SimpleFileJob::SimpleFileJob(AccountPtr account, const QString &filePath, QObject *parent)
    : AbstractNetworkJob(account, filePath, parent)
{
}

QNetworkReply *SimpleFileJob::startRequest(
    const QByteArray &verb, const QNetworkRequest req, QIODevice *requestBody)
{
    return startRequest(verb, makeDavUrl(path()), req, requestBody);
}

QNetworkReply *SimpleFileJob::startRequest(
    const QByteArray &verb, const QUrl &url, const QNetworkRequest req, QIODevice *requestBody)
{
    _verb = verb;
    const auto reply = sendRequest(verb, url, req, requestBody);

    if (reply->error() != QNetworkReply::NoError) {
        qCWarning(lcSimpleFileJob) << verb << " Network error: " << reply->errorString();
    }
    AbstractNetworkJob::start();
    return reply;
}

bool SimpleFileJob::finished()
{
    qCInfo(lcSimpleFileJob) << _verb << "for" << reply()->request().url() << "FINISHED WITH STATUS" << replyStatusString();
    emit finishedSignal(reply());
    return true;
}

DeleteApiJob::DeleteApiJob(AccountPtr account, const QString &path, QObject *parent)
    : SimpleFileJob(account, path, parent)
{

}

void DeleteApiJob::start()
{
    QNetworkRequest req;
    req.setRawHeader("OCS-APIREQUEST", "true");

    startRequest("DELETE", req);
}

bool DeleteApiJob::finished()
{
    qCInfo(lcJsonApiJob) << "DeleteApiJob of" << reply()->request().url() << "FINISHED WITH STATUS"
                         << reply()->error()
                         << (reply()->error() == QNetworkReply::NoError ? QLatin1String("") : errorString());

    int httpStatus = reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();


    if (reply()->error() != QNetworkReply::NoError) {
        qCWarning(lcJsonApiJob) << "Network error: " << path() << errorString() << httpStatus;
        emit result(httpStatus);
        return true;
    }

    const auto replyData = QString::fromUtf8(reply()->readAll());
    qCInfo(lcJsonApiJob()) << "TMX Delete Job" << replyData;
    emit result(httpStatus);
    return SimpleFileJob::finished();
}

void fetchPrivateLinkUrl(AccountPtr account, const QString &remotePath,
    const QByteArray &numericFileId, QObject *target,
    std::function<void(const QString &url)> targetFun)
{
    QString oldUrl;
    if (!numericFileId.isEmpty())
        oldUrl = account->deprecatedPrivateLinkUrl(numericFileId).toString(QUrl::FullyEncoded);

    // Retrieve the new link by PROPFIND
    auto *job = new PropfindJob(account, remotePath, target);
    job->setProperties(
        QList<QByteArray>()
        << "http://owncloud.org/ns:fileid" // numeric file id for fallback private link generation
        << "http://owncloud.org/ns:privatelink");
    job->setTimeout(10 * 1000);
    QObject::connect(job, &PropfindJob::result, target, [=](const QVariantMap &result) {
        auto privateLinkUrl = result["privatelink"].toString();
        auto numericFileId = result["fileid"].toByteArray();
        if (!privateLinkUrl.isEmpty()) {
            targetFun(privateLinkUrl);
        } else if (!numericFileId.isEmpty()) {
            targetFun(account->deprecatedPrivateLinkUrl(numericFileId).toString(QUrl::FullyEncoded));
        } else {
            targetFun(oldUrl);
        }
    });
    QObject::connect(job, &PropfindJob::finishedWithError, target, [=](QNetworkReply *) {
        targetFun(oldUrl);
    });
    job->start();
}

SimpleApiJob::SimpleApiJob(const AccountPtr &account, const QString &path, QObject *parent)
    : AbstractNetworkJob(account, path, parent)
{
}

void SimpleApiJob::setBody(const QByteArray &body)
{
    _body = body;
    qCDebug(lcSimpleApiJob) << "Set body for request:" << _body;
}


void SimpleApiJob::setVerb(Verb value)
{
    _verb = value;
}


QByteArray SimpleApiJob::verbToString() const
{
    switch (_verb) {
    case Verb::Get:
        return "GET";
    case Verb::Post:
        return "POST";
    case Verb::Put:
        return "PUT";
    case Verb::Delete:
        return "DELETE";
    }
    return "GET";
}

SimpleApiJob::Verb SimpleApiJob::stringToVerb(const QString &verb) const
{
    if (verb == QStringLiteral("POST")) {
        return Verb::Post;
    }

    if (verb == QStringLiteral("PUT")) {
        return Verb::Put;
    }

    if (verb == QStringLiteral("DELETE")) {
        return Verb::Delete;
    }

    return Verb::Get;
}

void SimpleApiJob::start()
{
    addRawHeader("OCS-APIREQUEST", "true");
    auto query = _additionalParams;
    QUrl url = Utility::concatUrlPath(account()->url(), path(), query);
    const auto httpVerb = verbToString();
    if (!SimpleApiJob::body().isEmpty()) {
        sendRequest(httpVerb, url, request(), SimpleApiJob::body());
    } else {
        sendRequest(httpVerb, url, request());
    }
    AbstractNetworkJob::start();
}

bool SimpleApiJob::finished()
{
    const auto httpStatusCode = reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    qCWarning(lcSimpleApiJob) << "result: " << path() << errorString() << httpStatusCode;
    emit resultReceived(httpStatusCode);
    return true;
}

QNetworkRequest& SimpleApiJob::request()
{
    return _request;
}

QByteArray& SimpleApiJob::body()
{
    return _body;
}

QUrlQuery &SimpleApiJob::additionalParams()
{
    return _additionalParams;
}

void SimpleApiJob::addQueryParams(const QUrlQuery &params)
{
    _additionalParams = params;
}

void SimpleApiJob::addRawHeader(const QByteArray &headerName, const QByteArray &value)
{
    request().setRawHeader(headerName, value);
}

} // namespace OCC
