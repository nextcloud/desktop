/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#include "syncenginetestutils.h"
#include "accessmanager.h"
#include "common/utility.h"
#include "gui/sharepermissions.h"
#include "httplogger.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QCryptographicHash>

#include <memory>
#include <filesystem>

PathComponents::PathComponents(const char *path)
    : PathComponents { QString::fromUtf8(path) }
{
}

PathComponents::PathComponents(const QString &path)
    : QStringList { path.split(QLatin1Char('/'), Qt::SkipEmptyParts) }
{
}

PathComponents::PathComponents(const QStringList &pathComponents)
    : QStringList { pathComponents }
{
}

PathComponents PathComponents::parentDirComponents() const
{
    return PathComponents { mid(0, size() - 1) };
}

PathComponents PathComponents::subComponents() const &
{
    return PathComponents { mid(1) };
}

void DiskFileModifier::remove(const QString &relativePath)
{
    QFileInfo fi { _rootDir.filePath(relativePath) };
    if (fi.isFile()) {
        QVERIFY(_rootDir.remove(relativePath));
    } else {
        const auto pathToDelete = fi.filePath();
        const auto result = OCC::FileSystem::removeRecursively(pathToDelete);
        if (!result) {
            qDebug() << "delete failed for:" << pathToDelete;
            QVERIFY(result);
        }
    }
}

void DiskFileModifier::insert(const QString &relativePath, qint64 size, char contentChar)
{
    QFile file { _rootDir.filePath(relativePath) };
    QVERIFY(!file.exists());
    file.open(QFile::WriteOnly);
    QByteArray buf(1024, contentChar);
    for (int x = 0; x < size / buf.size(); ++x) {
        file.write(buf);
    }
    file.write(buf.data(), size % buf.size());
    file.close();
    // Set the mtime 30 seconds in the past, for some tests that need to make sure that the mtime differs.
    OCC::FileSystem::setModTime(file.fileName(), OCC::Utility::qDateTimeToTime_t(QDateTime::currentDateTimeUtc().addSecs(-30)));
    if (file.size() != size) {
        QCOMPARE(file.size(), size);
    }
}

void DiskFileModifier::setContents(const QString &relativePath, char contentChar)
{
    QFile file { _rootDir.filePath(relativePath) };
    QVERIFY(file.exists());
    qint64 size = file.size();
    file.open(QFile::WriteOnly);
    file.write(QByteArray {}.fill(contentChar, size));
}

void DiskFileModifier::appendByte(const QString &relativePath)
{
    QFile file { _rootDir.filePath(relativePath) };
    QVERIFY(file.exists());
    file.open(QFile::ReadWrite);
    QByteArray contents = file.read(1);
    file.seek(file.size());
    file.write(contents);
}

void DiskFileModifier::mkdir(const QString &relativePath)
{
    _rootDir.mkpath(relativePath);
}

void DiskFileModifier::rename(const QString &from, const QString &to)
{
    QVERIFY(_rootDir.exists(from));
    const auto result = _rootDir.rename(from, to);
    if (!result) {
        qDebug() << "failed to rename from:" << from << "to:" << to;
        QVERIFY(result);
    }
}

void DiskFileModifier::setModTime(const QString &relativePath, const QDateTime &modTime)
{
    OCC::FileSystem::setModTime(_rootDir.filePath(relativePath), OCC::Utility::qDateTimeToTime_t(modTime));
}

void DiskFileModifier::modifyLockState([[maybe_unused]] const QString &relativePath, [[maybe_unused]] LockState lockState, [[maybe_unused]] int lockType, [[maybe_unused]] const QString &lockOwner, [[maybe_unused]] const QString &lockOwnerId, [[maybe_unused]] const QString &lockEditorId, [[maybe_unused]] quint64 lockTime, [[maybe_unused]] quint64 lockTimeout)
{
}

void DiskFileModifier::setE2EE([[maybe_unused]] const QString &relativePath, [[maybe_unused]] const bool enable)
{
}

QFile DiskFileModifier::find(const QString &relativePath) const
{
    const auto path = _rootDir.filePath(relativePath);
    return QFile(path);
}

FileInfo FileInfo::A12_B12_C12_S12()
{
    FileInfo fi { QString {}, {
                                  { QStringLiteral("A"), { { QStringLiteral("a1"), 4 }, { QStringLiteral("a2"), 4 } } },
                                  { QStringLiteral("B"), { { QStringLiteral("b1"), 16 }, { QStringLiteral("b2"), 16 } } },
                                  { QStringLiteral("C"), { { QStringLiteral("c1"), 24 }, { QStringLiteral("c2"), 24 } } },
                              } };
    FileInfo sharedFolder { QStringLiteral("S"), { { QStringLiteral("s1"), 32 }, { QStringLiteral("s2"), 32 } } };
    sharedFolder.isShared = true;
    sharedFolder.children[QStringLiteral("s1")].isShared = true;
    sharedFolder.children[QStringLiteral("s2")].isShared = true;
    fi.children.insert(sharedFolder.name, std::move(sharedFolder));
    return fi;
}

FileInfo::FileInfo(const QString &name, const std::initializer_list<FileInfo> &children)
    : name { name }
{
    for (const auto &source : children)
        addChild(source);
}

void FileInfo::addChild(const FileInfo &info)
{
    auto &dest = this->children[info.name] = info;
    dest.parentPath = path();
    dest.fixupParentPathRecursively();
}

void FileInfo::remove(const QString &relativePath)
{
    const PathComponents pathComponents { relativePath };
    FileInfo *parent = findInvalidatingEtags(pathComponents.parentDirComponents());
    Q_ASSERT(parent);
    auto childrenIt = std::find_if(parent->children.begin(), parent->children.end(),
                                   [&pathComponents](const FileInfo &fi) {
                                       return fi.name == pathComponents.fileName();
                                   });
    if (childrenIt != parent->children.end()) {
        parent->children.erase(childrenIt);
    }
}

void FileInfo::insert(const QString &relativePath, qint64 size, char contentChar)
{
    create(relativePath, size, contentChar);
}

void FileInfo::setContents(const QString &relativePath, char contentChar)
{
    FileInfo *file = findInvalidatingEtags(relativePath);
    Q_ASSERT(file);
    file->contentChar = contentChar;
    file->lastModified = QDateTime::currentDateTimeUtc();
}

void FileInfo::appendByte(const QString &relativePath)
{
    FileInfo *file = findInvalidatingEtags(relativePath);
    Q_ASSERT(file);
    file->size += 1;
}

void FileInfo::mkdir(const QString &relativePath)
{
    createDir(relativePath);
}

void FileInfo::rename(const QString &oldPath, const QString &newPath)
{
    const PathComponents newPathComponents { newPath };
    FileInfo *dir = findInvalidatingEtags(newPathComponents.parentDirComponents());
    Q_ASSERT(dir);
    Q_ASSERT(dir->isDir);
    const PathComponents pathComponents { oldPath };
    FileInfo *parent = findInvalidatingEtags(pathComponents.parentDirComponents());
    Q_ASSERT(parent);
    FileInfo fi = parent->children.take(pathComponents.fileName());
    fi.parentPath = dir->path();
    fi.name = newPathComponents.fileName();
    fi.fixupParentPathRecursively();
    dir->children.insert(newPathComponents.fileName(), std::move(fi));
}

void FileInfo::setModTime(const QString &relativePath, const QDateTime &modTime)
{
    FileInfo *file = findInvalidatingEtags(relativePath);
    Q_ASSERT(file);
    file->lastModified = modTime;
}

void FileInfo::setModTimeKeepEtag(const QString &relativePath, const QDateTime &modTime)
{
    FileInfo *file = find(relativePath);
    Q_ASSERT(file);
    file->lastModified = modTime;
}

void FileInfo::setIsLivePhoto(const QString &relativePath, const bool isLivePhoto)
{
    const auto file = find(relativePath);
    Q_ASSERT(file);
    file->isLivePhoto = isLivePhoto;
}

void FileInfo::modifyLockState(const QString &relativePath, LockState lockState, int lockType, const QString &lockOwner, const QString &lockOwnerId, const QString &lockEditorId, quint64 lockTime, quint64 lockTimeout)
{
    FileInfo *file = findInvalidatingEtags(relativePath);
    Q_ASSERT(file);
    file->lockState = lockState;
    file->lockType = lockType;
    file->lockOwner = lockOwner;
    file->lockOwnerId = lockOwnerId;
    file->lockEditorId = lockEditorId;
    file->lockTime = lockTime;
    file->lockTimeout = lockTimeout;
}

void FileInfo::setE2EE(const QString &relativePath, const bool enable)
{
    FileInfo *file = findInvalidatingEtags(relativePath);
    Q_ASSERT(file);
    file->isEncrypted = enable;
}

FileInfo *FileInfo::find(PathComponents pathComponents, const bool invalidateEtags)
{
    if (pathComponents.isEmpty()) {
        if (invalidateEtags) {
            etag = generateEtag();
        }
        return this;
    }
    QString childName = pathComponents.pathRoot();
    auto it = children.find(childName);
    if (it != children.end()) {
        auto file = it->find(std::move(pathComponents).subComponents(), invalidateEtags);
        if (file && invalidateEtags) {
            // Update parents on the way back
            etag = generateEtag();
        }
        return file;
    }
    return nullptr;
}

FileInfo FileInfo::findRecursive(PathComponents pathComponents, const bool invalidateEtags)
{
    auto result = find({pathComponents.takeFirst()}, invalidateEtags);
    if (!result) {
        return *result;
    }
    for (const auto &pathComponent : pathComponents) {
        if (!result) {
            break;
        }
        result = result->find({pathComponent});
    }
    return *result;
}

FileInfo *FileInfo::createDir(const QString &relativePath)
{
    const PathComponents pathComponents { relativePath };
    FileInfo *parent = findInvalidatingEtags(pathComponents.parentDirComponents());
    Q_ASSERT(parent);
    FileInfo &child = parent->children[pathComponents.fileName()] = FileInfo { pathComponents.fileName() };
    child.parentPath = parent->path();
    child.etag = generateEtag();
    return &child;
}

FileInfo *FileInfo::create(const QString &relativePath, qint64 size, char contentChar)
{
    const PathComponents pathComponents { relativePath };
    FileInfo *parent = findInvalidatingEtags(pathComponents.parentDirComponents());
    Q_ASSERT(parent);
    FileInfo &child = parent->children[pathComponents.fileName()] = FileInfo { pathComponents.fileName(), size };
    child.parentPath = parent->path();
    child.contentChar = contentChar;
    child.etag = generateEtag();
    return &child;
}

bool FileInfo::operator==(const FileInfo &other) const
{
    // Consider files to be equal between local<->remote as a user would.
    return name == other.name
        && isDir == other.isDir
        && size == other.size
        && contentChar == other.contentChar
        && children == other.children;
}

QString FileInfo::path() const
{
    return (parentPath.isEmpty() ? QString() : (parentPath + QLatin1Char('/'))) + name;
}

QString FileInfo::absolutePath() const
{
    return OCC::Utility::trailingSlashPath(parentPath) + name;
}

void FileInfo::fixupParentPathRecursively()
{
    auto p = path();
    for (auto it = children.begin(); it != children.end(); ++it) {
        Q_ASSERT(it.key() == it->name);
        it->parentPath = p;
        it->fixupParentPathRecursively();
    }
}

FileInfo *FileInfo::findInvalidatingEtags(PathComponents pathComponents)
{
    return find(std::move(pathComponents), true);
}

FakePropfindReply::FakePropfindReply(FileInfo &remoteRootFileInfo, QNetworkAccessManager::Operation op, const QNetworkRequest &request, QObject *parent)
    : FakeReply { parent }
{
    setRequest(request);
    setUrl(request.url());
    setOperation(op);
    open(QIODevice::ReadOnly);

    QString fileName = getFilePathFromUrl(request.url());
    Q_ASSERT(!fileName.isNull()); // for root, it should be empty
    const FileInfo *fileInfo = remoteRootFileInfo.find(fileName);
    if (!fileInfo) {
        QMetaObject::invokeMethod(this, "respond404", Qt::QueuedConnection);
        return;
    }
    const QString prefix = request.url().path().left(request.url().path().size() - fileName.size());

    // Don't care about the request and just return a full propfind
    const QString davUri { QStringLiteral("DAV:") };
    const QString ocUri { QStringLiteral("http://owncloud.org/ns") };
    const QString ncUri { QStringLiteral("http://nextcloud.org/ns") };
    QBuffer buffer { &payload };
    buffer.open(QIODevice::WriteOnly);
    QXmlStreamWriter xml(&buffer);
    xml.writeNamespace(davUri, QStringLiteral("d"));
    xml.writeNamespace(ocUri, QStringLiteral("oc"));
    xml.writeNamespace(ncUri, QStringLiteral("nc"));
    xml.writeStartDocument();
    xml.writeStartElement(davUri, QStringLiteral("multistatus"));
    auto writeFileResponse = [&](const FileInfo &fileInfo) {
        xml.writeStartElement(davUri, QStringLiteral("response"));

        const auto url = OCC::Utility::trailingSlashPath(QString::fromUtf8(QUrl::toPercentEncoding(fileInfo.absolutePath(), "/")));
        const auto href = OCC::Utility::concatUrlPath(prefix, url).path();
        xml.writeTextElement(davUri, QStringLiteral("href"), href);
        xml.writeStartElement(davUri, QStringLiteral("propstat"));
        xml.writeStartElement(davUri, QStringLiteral("prop"));

        if (fileInfo.isDir) {
            xml.writeStartElement(davUri, QStringLiteral("resourcetype"));
            xml.writeEmptyElement(davUri, QStringLiteral("collection"));
            xml.writeEndElement(); // resourcetype

            auto totalSize = 0;
            for (const auto &child : fileInfo.children.values()) {
                totalSize += child.size;
            }
            xml.writeTextElement(ocUri, QStringLiteral("size"), QString::number(totalSize));
        } else
            xml.writeEmptyElement(davUri, QStringLiteral("resourcetype"));

        auto gmtDate = fileInfo.lastModified.toUTC();
        auto stringDate = QLocale::c().toString(gmtDate, QStringLiteral("ddd, dd MMM yyyy HH:mm:ss 'GMT'"));
        xml.writeTextElement(davUri, QStringLiteral("getlastmodified"), stringDate);
        xml.writeTextElement(davUri, QStringLiteral("getcontentlength"), QString::number(fileInfo.size));
        xml.writeTextElement(davUri, QStringLiteral("getetag"), QStringLiteral("\"%1\"").arg(QString::fromLatin1(fileInfo.etag)));
        xml.writeTextElement(ocUri, QStringLiteral("permissions"), !fileInfo.permissions.isNull() ? QString(fileInfo.permissions.toString()) : fileInfo.isShared ? QStringLiteral("GSRDNVCKW") : QStringLiteral("GRDNVCKW"));
        xml.writeTextElement(ocUri, QStringLiteral("share-permissions"), QString::number(static_cast<int>(OCC::SharePermissions(OCC::SharePermissionRead |
                                                                                                                                OCC::SharePermissionUpdate |
                                                                                                                                OCC::SharePermissionCreate |
                                                                                                                                OCC::SharePermissionDelete |
                                                                                                                                OCC::SharePermissionShare))));
        xml.writeTextElement(ocUri, QStringLiteral("id"), QString::fromUtf8(fileInfo.fileId));
        xml.writeTextElement(ocUri, QStringLiteral("fileid"), QString::fromUtf8(fileInfo.fileId));
        xml.writeTextElement(ocUri, QStringLiteral("checksums"), QString::fromUtf8(fileInfo.checksums));
        xml.writeTextElement(ocUri, QStringLiteral("privatelink"), href);
        xml.writeTextElement(ncUri, QStringLiteral("lock-owner"), fileInfo.lockOwnerId);
        xml.writeTextElement(ncUri, QStringLiteral("lock"), fileInfo.lockState == FileInfo::LockState::FileLocked ? QStringLiteral("1") : QStringLiteral("0"));
        xml.writeTextElement(ncUri, QStringLiteral("lock-owner-type"), fileInfo.lockOwnerId);
        xml.writeTextElement(ncUri, QStringLiteral("lock-owner-displayname"), fileInfo.lockOwnerId);
        xml.writeTextElement(ncUri, QStringLiteral("lock-owner-editor"), fileInfo.lockOwnerId);
        xml.writeTextElement(ncUri, QStringLiteral("lock-time"), QString::number(fileInfo.lockTime));
        xml.writeTextElement(ncUri, QStringLiteral("lock-timeout"), QString::number(fileInfo.lockTimeout));
        xml.writeTextElement(ncUri, QStringLiteral("is-encrypted"), fileInfo.isEncrypted ? QString::number(1) : QString::number(0));
        xml.writeTextElement(ncUri, QStringLiteral("metadata-files-live-photo"), fileInfo.isLivePhoto ? QString::number(1) : QString::number(0));
        buffer.write(fileInfo.extraDavProperties);
        xml.writeEndElement(); // prop
        xml.writeTextElement(davUri, QStringLiteral("status"), QStringLiteral("HTTP/1.1 200 OK"));
        xml.writeEndElement(); // propstat
        xml.writeEndElement(); // response
    };

    writeFileResponse(*fileInfo);
    foreach (const FileInfo &childFileInfo, fileInfo->children)
        writeFileResponse(childFileInfo);
    xml.writeEndElement(); // multistatus
    xml.writeEndDocument();

    QMetaObject::invokeMethod(this, "respond", Qt::QueuedConnection);
}

FakePropfindReply::FakePropfindReply(const QByteArray &replyContents, QNetworkAccessManager::Operation op, const QNetworkRequest &request, QObject *parent)
    : FakeReply { parent }
{
    setRequest(request);
    setUrl(request.url());
    setOperation(op);

    payload = replyContents;

    QMetaObject::invokeMethod(this, "respond", Qt::QueuedConnection);
}

void FakePropfindReply::respond()
{
    setHeader(QNetworkRequest::ContentLengthHeader, payload.size());
    setHeader(QNetworkRequest::ContentTypeHeader, QByteArrayLiteral("application/xml; charset=utf-8"));
    setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 207);
    setFinished(true);
    emit metaDataChanged();
    if (bytesAvailable())
        emit readyRead();
    emit finished();
}

void FakePropfindReply::respond404()
{
    setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 404);
    setError(InternalServerError, QStringLiteral("Not Found"));
    emit metaDataChanged();
    emit finished();
}

qint64 FakePropfindReply::bytesAvailable() const
{
    return payload.size() + QIODevice::bytesAvailable();
}

qint64 FakePropfindReply::readData(char *data, qint64 maxlen)
{
    qint64 len = std::min(qint64 { payload.size() }, maxlen);
    std::copy(payload.cbegin(), payload.cbegin() + len, data);
    payload.remove(0, static_cast<int>(len));
    return len;
}

FakePutReply::FakePutReply(FileInfo &remoteRootFileInfo, QNetworkAccessManager::Operation op, const QNetworkRequest &request, const QByteArray &putPayload, QObject *parent)
    : FakeReply { parent }
{
    setRequest(request);
    setUrl(request.url());
    setOperation(op);
    open(QIODevice::ReadOnly);
    fileInfo = perform(remoteRootFileInfo, request, putPayload);
    QMetaObject::invokeMethod(this, "respond", Qt::QueuedConnection);
}

FileInfo *FakePutReply::perform(FileInfo &remoteRootFileInfo, const QNetworkRequest &request, const QByteArray &putPayload)
{
    QString fileName = getFilePathFromUrl(request.url());
    Q_ASSERT(!fileName.isEmpty());
    FileInfo *fileInfo = remoteRootFileInfo.find(fileName);
    if (fileInfo) {
        fileInfo->size = putPayload.size();
        fileInfo->contentChar = putPayload.at(0);
    } else {
        // Assume that the file is filled with the same character
        fileInfo = remoteRootFileInfo.create(fileName, putPayload.size(), putPayload.isEmpty() ? ' ' : putPayload.at(0));
    }
    fileInfo->lastModified = OCC::Utility::qDateTimeFromTime_t(request.rawHeader("X-OC-Mtime").toLongLong());
    remoteRootFileInfo.find(fileName, /*invalidateEtags=*/true);
    return fileInfo;
}

void FakePutReply::respond()
{
    emit uploadProgress(fileInfo->size, fileInfo->size);
    setRawHeader("OC-ETag", fileInfo->etag);
    setRawHeader("ETag", fileInfo->etag);
    setRawHeader("OC-FileID", fileInfo->fileId);
    setRawHeader("X-OC-MTime", "accepted"); // Prevents Q_ASSERT(!_runningNow) since we'll call PropagateItemJob::done twice in that case.
    setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);
    emit metaDataChanged();
    emit finished();
}

void FakePutReply::abort()
{
    setError(OperationCanceledError, QStringLiteral("abort"));
    emit finished();
}

FakePutMultiFileReply::FakePutMultiFileReply(FileInfo &remoteRootFileInfo, QNetworkAccessManager::Operation op, const QNetworkRequest &request, const QString &contentType, const QByteArray &putPayload, const QString &serverVersion, QObject *parent)
    : FakeReply { parent }
    , _serverVersion(serverVersion)
{
    setRequest(request);
    setUrl(request.url());
    setOperation(op);
    open(QIODevice::ReadOnly);
    _allFileInfo = performMultiPart(remoteRootFileInfo, request, putPayload, contentType, _serverVersion);
    QMetaObject::invokeMethod(this, "respond", Qt::QueuedConnection);
}

QVector<FileInfo *> FakePutMultiFileReply::performMultiPart(FileInfo &remoteRootFileInfo, const QNetworkRequest &request, const QByteArray &putPayload, const QString &contentType, const QString &serverVersion)
{
    Q_UNUSED(request)
    QVector<FileInfo *> result;

    auto stringPutPayload = QString::fromUtf8(putPayload);
    constexpr int boundaryPosition = sizeof("multipart/related; boundary=");
    const QString boundaryValue = QStringLiteral("--") + contentType.mid(boundaryPosition, contentType.length() - boundaryPosition - 1) + QStringLiteral("\r\n");
    auto stringPutPayloadRef = QString{stringPutPayload}.left(stringPutPayload.size() - 2 - boundaryValue.size());
    auto allParts = stringPutPayloadRef.split(boundaryValue, Qt::SkipEmptyParts);
    for (const auto &onePart : allParts) {
        auto headerEndPosition = onePart.indexOf(QStringLiteral("\r\n\r\n"));
        auto onePartHeaderPart = onePart.left(headerEndPosition);
        auto onePartBody = onePart.mid(headerEndPosition + 4, onePart.size() - headerEndPosition - 6);
        auto onePartHeaders = onePartHeaderPart.split(QStringLiteral("\r\n"));
        QMap<QString, QString> allHeaders;
        for(const auto &oneHeader : onePartHeaders) {
            auto headerParts = oneHeader.split(QStringLiteral(": "));
            allHeaders[headerParts.at(0).toLower()] = headerParts.at(1);
        }
        const auto fileName = allHeaders[QStringLiteral("x-file-path")];
        const auto modtime = allHeaders[QByteArrayLiteral("x-file-mtime")].toLongLong();
        const auto expectedMd5Checksum = allHeaders[QStringLiteral("x-file-md5")];
        const auto standardChecksum = allHeaders[QStringLiteral("oc-checksum")];
        Q_ASSERT(!fileName.isEmpty());
        Q_ASSERT(modtime > 0);

        auto components = serverVersion.split('.');
        const auto serverIntVersion = OCC::Account::makeServerVersion(components.value(0).toInt(),
                                                                      components.value(1).toInt(),
                                                                      components.value(2).toInt());

        const auto md5ChecksumMandatory = serverIntVersion < OCC::Account::makeServerVersion(32, 0, 0);

        if (md5ChecksumMandatory) {
            Q_ASSERT(!expectedMd5Checksum.isEmpty());
        } else {
            Q_ASSERT(expectedMd5Checksum.isEmpty());
        }
        Q_ASSERT(!standardChecksum.isEmpty());

        const auto standardChecksumComponents = standardChecksum.split(':');
        Q_ASSERT(standardChecksumComponents.size() == 2);
        auto standardHashAlgorithm = QCryptographicHash::Algorithm::Sha1;
        const auto standardHashAlgorithmString = standardChecksumComponents.at(0);
        if (standardHashAlgorithmString == QStringLiteral("MD5")) {
            standardHashAlgorithm = QCryptographicHash::Algorithm::Md5;
        } else if (standardHashAlgorithmString == QStringLiteral("SHA1")) {
            standardHashAlgorithm = QCryptographicHash::Algorithm::Sha1;
        } else if (standardHashAlgorithmString == QStringLiteral("SHA256")) {
            standardHashAlgorithm = QCryptographicHash::Algorithm::Sha256;
        } else if (standardHashAlgorithmString == QStringLiteral("SHA3_256")) {
            standardHashAlgorithm = QCryptographicHash::Algorithm::Sha3_256;
        } else if (standardHashAlgorithmString == QStringLiteral("Adler32")) {
            Q_ASSERT(false);
        }

        if (md5ChecksumMandatory) {
            QCryptographicHash md5SumAlgorithm{QCryptographicHash::Algorithm::Md5};

            md5SumAlgorithm.addData(onePartBody.toLatin1());
            const auto computedMd5Checksum = md5SumAlgorithm.result().toHex();
            Q_ASSERT(expectedMd5Checksum == computedMd5Checksum);
        }

        QCryptographicHash standardSumAlgorithm{standardHashAlgorithm};

        standardSumAlgorithm.addData(onePartBody.toLatin1());
        const auto computedStandardChecksum = standardSumAlgorithm.result().toHex();
        Q_ASSERT(standardChecksumComponents.at(1) == computedStandardChecksum);

        FileInfo *fileInfo = remoteRootFileInfo.find(fileName);
        if (fileInfo) {
            fileInfo->size = onePartBody.size();
            fileInfo->contentChar = onePartBody.at(0).toLatin1();
        } else {
            // Assume that the file is filled with the same character
            fileInfo = remoteRootFileInfo.create(fileName, onePartBody.size(), onePartBody.at(0).toLatin1());
        }
        fileInfo->lastModified = OCC::Utility::qDateTimeFromTime_t(modtime);
        remoteRootFileInfo.find(fileName, /*invalidateEtags=*/true);
        result.push_back(fileInfo);
    }
    return result;
}

void FakePutMultiFileReply::respond()
{
    QJsonDocument reply;
    QJsonObject allFileInfoReply;

    qint64 totalSize = 0;
    std::for_each(_allFileInfo.begin(), _allFileInfo.end(), [&totalSize](const auto &fileInfo) {
        totalSize += fileInfo->size;
    });

    for(auto fileInfo : std::as_const(_allFileInfo)) {
        QJsonObject fileInfoReply;
        fileInfoReply.insert("error", QStringLiteral("false"));
        fileInfoReply.insert("etag", QLatin1String{fileInfo->etag});
        emit uploadProgress(fileInfo->size, totalSize);
        allFileInfoReply.insert(QChar('/') + fileInfo->path(), fileInfoReply);
    }
    reply.setObject(allFileInfoReply);
    _payload = reply.toJson();

    setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);

    setFinished(true);
    if (bytesAvailable()) {
        emit readyRead();
    }

    emit metaDataChanged();
    emit finished();
}

void FakePutMultiFileReply::abort()
{
    setError(OperationCanceledError, QStringLiteral("abort"));
    emit finished();
}

qint64 FakePutMultiFileReply::bytesAvailable() const
{
    return _payload.size() + QIODevice::bytesAvailable();
}

qint64 FakePutMultiFileReply::readData(char *data, qint64 maxlen)
{
    qint64 len = std::min(qint64 { _payload.size() }, maxlen);
    std::copy(_payload.cbegin(), _payload.cbegin() + len, data);
    _payload.remove(0, static_cast<int>(len));
    return len;
}

FakeMkcolReply::FakeMkcolReply(FileInfo &remoteRootFileInfo, QNetworkAccessManager::Operation op, const QNetworkRequest &request, QObject *parent)
    : FakeReply { parent }
{
    setRequest(request);
    setUrl(request.url());
    setOperation(op);
    open(QIODevice::ReadOnly);

    QString fileName = getFilePathFromUrl(request.url());
    Q_ASSERT(!fileName.isEmpty());
    fileInfo = remoteRootFileInfo.createDir(fileName);

    if (!fileInfo) {
        abort();
        return;
    }
    QMetaObject::invokeMethod(this, "respond", Qt::QueuedConnection);
}

void FakeMkcolReply::respond()
{
    setRawHeader("OC-FileId", fileInfo->fileId);
    setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 201);
    emit metaDataChanged();
    emit finished();
}

FakeDeleteReply::FakeDeleteReply(FileInfo &remoteRootFileInfo, QNetworkAccessManager::Operation op, const QNetworkRequest &request, QObject *parent)
    : FakeReply { parent }
{
    setRequest(request);
    setUrl(request.url());
    setOperation(op);
    open(QIODevice::ReadOnly);

    QString fileName = getFilePathFromUrl(request.url());
    Q_ASSERT(!fileName.isEmpty());
    remoteRootFileInfo.remove(fileName);
    QMetaObject::invokeMethod(this, "respond", Qt::QueuedConnection);
}

void FakeDeleteReply::respond()
{
    setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 204);
    emit metaDataChanged();
    emit finished();
}

FakeMoveReply::FakeMoveReply(FileInfo &remoteRootFileInfo, QNetworkAccessManager::Operation op, const QNetworkRequest &request, QObject *parent)
    : FakeReply { parent }
{
    setRequest(request);
    setUrl(request.url());
    setOperation(op);
    open(QIODevice::ReadOnly);

    QString fileName = getFilePathFromUrl(request.url());
    Q_ASSERT(!fileName.isEmpty());
    QString dest = getFilePathFromUrl(QUrl::fromEncoded(request.rawHeader("Destination")));
    Q_ASSERT(!dest.isEmpty());
    remoteRootFileInfo.rename(fileName, dest);
    QMetaObject::invokeMethod(this, "respond", Qt::QueuedConnection);
}

void FakeMoveReply::respond()
{
    setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 201);
    emit metaDataChanged();
    emit finished();
}

FakeGetReply::FakeGetReply(FileInfo &remoteRootFileInfo, QNetworkAccessManager::Operation op, const QNetworkRequest &request, QObject *parent)
    : FakeReply { parent }
{
    setRequest(request);
    setUrl(request.url());
    setOperation(op);
    open(QIODevice::ReadOnly);

    QString fileName = getFilePathFromUrl(request.url());
    Q_ASSERT(!fileName.isEmpty());
    fileInfo = remoteRootFileInfo.find(fileName);
    if (!fileInfo) {
        qDebug() << "url: " << request.url() << " fileName: " << fileName
                 << " meh;";
    }
    Q_ASSERT_X(fileInfo, Q_FUNC_INFO, "Could not find file on the remote");
    QMetaObject::invokeMethod(this, &FakeGetReply::respond, Qt::QueuedConnection);
}

void FakeGetReply::respond()
{
    if (aborted) {
        setError(OperationCanceledError, QStringLiteral("Operation Canceled"));
        emit metaDataChanged();
        emit finished();
        return;
    }
    if (!fileInfo) {
        setError(ContentNotFoundError, QStringLiteral("File Not Found"));
        emit metaDataChanged();
        emit finished();
        return;
    }
    payload = fileInfo->contentChar;
    size = fileInfo->size;
    setHeader(QNetworkRequest::ContentLengthHeader, size);
    setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);
    setRawHeader("OC-ETag", fileInfo->etag);
    setRawHeader("ETag", fileInfo->etag);
    setRawHeader("OC-FileId", fileInfo->fileId);
    emit metaDataChanged();
    if (bytesAvailable())
        emit readyRead();
    emit finished();
}

void FakeGetReply::abort()
{
    setError(OperationCanceledError, QStringLiteral("Operation Canceled"));
    aborted = true;
}

qint64 FakeGetReply::bytesAvailable() const
{
    if (aborted)
        return 0;
    return size + QIODevice::bytesAvailable();
}

qint64 FakeGetReply::readData(char *data, qint64 maxlen)
{
    qint64 len = std::min(qint64 { size }, maxlen);
    std::fill_n(data, len, payload);
    size -= len;
    return len;
}

FakeGetWithDataReply::FakeGetWithDataReply(FileInfo &remoteRootFileInfo, const QByteArray &data, QNetworkAccessManager::Operation op, const QNetworkRequest &request, QObject *parent)
    : FakeReply { parent }
{
    setRequest(request);
    setUrl(request.url());
    setOperation(op);
    open(QIODevice::ReadOnly);

    Q_ASSERT(!data.isEmpty());
    payload = data;
    QString fileName = getFilePathFromUrl(request.url());
    Q_ASSERT(!fileName.isEmpty());
    fileInfo = remoteRootFileInfo.find(fileName);
    QMetaObject::invokeMethod(this, "respond", Qt::QueuedConnection);

    if (request.hasRawHeader("Range")) {
        const QString range = QString::fromUtf8(request.rawHeader("Range"));
        const QRegularExpression bytesPattern(QStringLiteral("bytes=(?<start>\\d+)-(?<end>\\d+)"));
        const QRegularExpressionMatch match = bytesPattern.match(range);
        if (match.hasMatch()) {
            const int start = match.captured(QStringLiteral("start")).toInt();
            const int end = match.captured(QStringLiteral("end")).toInt();
            payload = payload.mid(start, end - start + 1);
        }
    }
}

void FakeGetWithDataReply::respond()
{
    if (aborted) {
        setError(OperationCanceledError, QStringLiteral("Operation Canceled"));
        emit metaDataChanged();
        emit finished();
        return;
    }
    setHeader(QNetworkRequest::ContentLengthHeader, payload.size());
    setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);
    setRawHeader("OC-ETag", fileInfo->etag);
    setRawHeader("ETag", fileInfo->etag);
    setRawHeader("OC-FileId", fileInfo->fileId);
    emit metaDataChanged();
    if (bytesAvailable())
        emit readyRead();
    emit finished();
}

void FakeGetWithDataReply::abort()
{
    setError(OperationCanceledError, QStringLiteral("Operation Canceled"));
    aborted = true;
}

qint64 FakeGetWithDataReply::bytesAvailable() const
{
    if (aborted)
        return 0;
    return payload.size() - offset + QIODevice::bytesAvailable();
}

qint64 FakeGetWithDataReply::readData(char *data, qint64 maxlen)
{
    qint64 len = std::min(payload.size() - offset, quint64(maxlen));
    std::memcpy(data, payload.constData() + offset, len);
    offset += len;
    return len;
}

FakeChunkMoveReply::FakeChunkMoveReply(FileInfo &uploadsFileInfo, FileInfo &remoteRootFileInfo, QNetworkAccessManager::Operation op, const QNetworkRequest &request, QObject *parent)
    : FakeReply { parent }
{
    setRequest(request);
    setUrl(request.url());
    setOperation(op);
    open(QIODevice::ReadOnly);
    fileInfo = perform(uploadsFileInfo, remoteRootFileInfo, request);
    if (!fileInfo) {
        QTimer::singleShot(0, this, &FakeChunkMoveReply::respondPreconditionFailed);
    } else {
        QTimer::singleShot(0, this, &FakeChunkMoveReply::respond);
    }
}

FileInfo *FakeChunkMoveReply::perform(FileInfo &uploadsFileInfo, FileInfo &remoteRootFileInfo, const QNetworkRequest &request)
{
    QString source = getFilePathFromUrl(request.url());
    Q_ASSERT(!source.isEmpty());
    Q_ASSERT(source.endsWith(QLatin1String("/.file")));
    source = source.left(source.length() - static_cast<int>(qstrlen("/.file")));

    auto sourceFolder = uploadsFileInfo.find(source);
    Q_ASSERT(sourceFolder);
    Q_ASSERT(sourceFolder->isDir);
    int count = 0;
    qlonglong size = 0;
    char payload = '\0';

    QString fileName = getFilePathFromUrl(QUrl::fromEncoded(request.rawHeader("Destination")));
    Q_ASSERT(!fileName.isEmpty());

    // Compute the size and content from the chunks if possible
    const auto childrenKeys = sourceFolder->children.keys();
    for (auto chunkName : childrenKeys) {
        auto &x = sourceFolder->children[chunkName];
        Q_ASSERT(!x.isDir);
        Q_ASSERT(x.size > 0); // There should not be empty chunks
        size += x.size;
        Q_ASSERT(!payload || payload == x.contentChar);
        payload = x.contentChar;
        ++count;
    }
    Q_ASSERT(sourceFolder->children.count() == count); // There should not be holes or extra files

    // NOTE: This does not actually assemble the file data from the chunks!
    FileInfo *fileInfo = remoteRootFileInfo.find(fileName);
    if (fileInfo) {
        // The client should put this header
        Q_ASSERT(request.hasRawHeader("If"));

        // And it should condition on the destination file
        auto start = QByteArray("<" + request.rawHeader("Destination") + ">");
        Q_ASSERT(request.rawHeader("If").startsWith(start));

        if (request.rawHeader("If") != start + " ([\"" + fileInfo->etag + "\"])") {
            return nullptr;
        }
        fileInfo->size = size;
        fileInfo->contentChar = payload;
    } else {
        Q_ASSERT(!request.hasRawHeader("If"));
        // Assume that the file is filled with the same character
        fileInfo = remoteRootFileInfo.create(fileName, size, payload);
    }
    fileInfo->lastModified = OCC::Utility::qDateTimeFromTime_t(request.rawHeader("X-OC-Mtime").toLongLong());
    remoteRootFileInfo.find(fileName, /*invalidateEtags=*/true);

    return fileInfo;
}

void FakeChunkMoveReply::respond()
{
    setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 201);
    setRawHeader("OC-ETag", fileInfo->etag);
    setRawHeader("ETag", fileInfo->etag);
    setRawHeader("OC-FileId", fileInfo->fileId);
    emit metaDataChanged();
    emit finished();
}

void FakeChunkMoveReply::respondPreconditionFailed()
{
    setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 412);
    setError(InternalServerError, QStringLiteral("Precondition Failed"));
    emit metaDataChanged();
    emit finished();
}

void FakeChunkMoveReply::abort()
{
    setError(OperationCanceledError, QStringLiteral("abort"));
    emit finished();
}

FakePayloadReply::FakePayloadReply(QNetworkAccessManager::Operation op, const QNetworkRequest &request, const QByteArray &body, QObject *parent)
    : FakePayloadReply(op, request, body, FakePayloadReply::defaultDelay, parent)
{
}

FakePayloadReply::FakePayloadReply(
    QNetworkAccessManager::Operation op, const QNetworkRequest &request, const QByteArray &body, int delay, QObject *parent)
    : FakeReply{parent}
    , _body(body)
{
    setRequest(request);
    setUrl(request.url());
    setOperation(op);
    open(QIODevice::ReadOnly);
    QTimer::singleShot(delay, this, &FakePayloadReply::respond);
}

void FakePayloadReply::respond()
{
    setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);
    setHeader(QNetworkRequest::ContentLengthHeader, _body.size());
    for (auto it = _additionalHeaders.constKeyValueBegin(); it != _additionalHeaders.constKeyValueEnd(); ++it) {
        setHeader(it->first, it->second);
    }
    emit metaDataChanged();
    emit readyRead();
    setFinished(true);
    emit finished();
}

qint64 FakePayloadReply::readData(char *buf, qint64 max)
{
    max = qMin<qint64>(max, _body.size());
    memcpy(buf, _body.constData(), max);
    _body = _body.mid(max);
    return max;
}

qint64 FakePayloadReply::bytesAvailable() const
{
    return _body.size();
}

FakeErrorReply::FakeErrorReply(QNetworkAccessManager::Operation op, const QNetworkRequest &request, QObject *parent, int httpErrorCode, const QByteArray &body)
    : FakeReply { parent }
    , _body(body)
{
    setRequest(request);
    setUrl(request.url());
    setOperation(op);
    open(QIODevice::ReadOnly);
    setAttribute(QNetworkRequest::HttpStatusCodeAttribute, httpErrorCode);
    setError(InternalServerError, QStringLiteral("Internal Server Fake Error"));
    QMetaObject::invokeMethod(this, &FakeErrorReply::respond, Qt::QueuedConnection);
}

void FakeErrorReply::respond()
{
    emit metaDataChanged();
    emit readyRead();
    // finishing can come strictly after readyRead was called
    QTimer::singleShot(5, this, &FakeErrorReply::slotSetFinished);
}

void FakeErrorReply::slotSetFinished()
{
    setFinished(true);
    emit finished();
}

qint64 FakeErrorReply::readData(char *buf, qint64 max)
{
    max = qMin<qint64>(max, _body.size());
    memcpy(buf, _body.constData(), max);
    _body = _body.mid(max);
    return max;
}

qint64 FakeErrorReply::bytesAvailable() const
{
    return _body.size();
}

FakeHangingReply::FakeHangingReply(QNetworkAccessManager::Operation op, const QNetworkRequest &request, QObject *parent)
    : FakeReply(parent)
{
    setRequest(request);
    setUrl(request.url());
    setOperation(op);
    open(QIODevice::ReadOnly);
}

void FakeHangingReply::abort()
{
    // Follow more or less the implementation of QNetworkReplyImpl::abort
    close();
    setError(OperationCanceledError, tr("Operation canceled"));
    emit errorOccurred(OperationCanceledError);
    setFinished(true);
    emit finished();
}

FakeQNAM::FakeQNAM(FileInfo initialRoot)
    : _remoteRootFileInfo { std::move(initialRoot) }
{
    setCookieJar(new OCC::CookieJar);
}

QJsonObject FakeQNAM::forEachReplyPart(QIODevice *outgoingData,
                                       const QString &contentType,
                                       std::function<QJsonObject (const QMap<QString, QByteArray> &)> replyFunction)
{
    auto fullReply = QJsonObject{};
    auto putPayload = outgoingData->peek(outgoingData->bytesAvailable());
    outgoingData->reset();
    auto stringPutPayload = QString::fromUtf8(putPayload);
    constexpr int boundaryPosition = sizeof("multipart/related; boundary=");
    const QString boundaryValue = QStringLiteral("--") + contentType.mid(boundaryPosition, contentType.length() - boundaryPosition - 1) + QStringLiteral("\r\n");
    auto stringPutPayloadRef = QString{stringPutPayload}.left(stringPutPayload.size() - 2 - boundaryValue.size());
    auto allParts = stringPutPayloadRef.split(boundaryValue, Qt::SkipEmptyParts);
    for (const auto &onePart : std::as_const(allParts)) {
        auto headerEndPosition = onePart.indexOf(QStringLiteral("\r\n\r\n"));
        auto onePartHeaderPart = onePart.left(headerEndPosition);
        auto onePartHeaders = onePartHeaderPart.split(QStringLiteral("\r\n"));
        QMap<QString, QByteArray> allHeaders;
        for(const auto &oneHeader : std::as_const(onePartHeaders)) {
            auto headerParts = oneHeader.split(QStringLiteral(": "));
            allHeaders[headerParts.at(0).toLower()] = headerParts.at(1).toLatin1();
        }

        auto reply = replyFunction(allHeaders);
        if (reply.contains(QStringLiteral("error")) &&
                reply.contains(QStringLiteral("etag"))) {
            fullReply.insert(allHeaders[QStringLiteral("x-file-path")], reply);
        }
    }

    return fullReply;
}

QNetworkReply *FakeQNAM::createRequest(QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *outgoingData)
{
    if (op == QNetworkAccessManager::CustomOperation) {
        qInfo() << "Operation" << request.attribute(QNetworkRequest::CustomVerbAttribute).toString() << request.url();
    } else {
        qInfo() << "Operation" << op << request.url();
    }
    QNetworkReply *reply = nullptr;
    auto newRequest = request;
    newRequest.setRawHeader("X-Request-ID", OCC::AccessManager::generateRequestId());
    auto contentType = request.header(QNetworkRequest::ContentTypeHeader).toString();
    if (_override) {
        qDebug() << "Using override!";
        if (auto _reply = _override(op, newRequest, outgoingData)) {
            reply = _reply;
        }
    }
    if (!reply) {
        qDebug() << newRequest.url();
        reply = overrideReplyWithError(getFilePathFromUrl(newRequest.url()), op, newRequest);
    }
    if (!reply) {
        const bool isUpload = newRequest.url().path().startsWith(sUploadUrl.path());
        FileInfo &info = isUpload ? _uploadFileInfo : _remoteRootFileInfo;

        auto verb = newRequest.attribute(QNetworkRequest::CustomVerbAttribute).toString();
        if (verb == QLatin1String("PROPFIND")) {
            // Ignore outgoingData always returning something good enough, works for now.
            reply = new FakePropfindReply { info, op, newRequest, this };
        } else if (verb == QLatin1String("GET") || op == QNetworkAccessManager::GetOperation) {
            reply = new FakeGetReply { info, op, newRequest, this };
        } else if (verb == QLatin1String("PUT") || op == QNetworkAccessManager::PutOperation) {
            if (request.hasRawHeader(QByteArrayLiteral("X-OC-Mtime")) &&
                    request.rawHeader(QByteArrayLiteral("X-OC-Mtime")).toLongLong() <= 0) {
                reply = new FakeErrorReply { op, request, this, 500 };
            } else {
                reply = new FakePutReply { info, op, newRequest, outgoingData->readAll(), this };
            }
        } else if (verb == QLatin1String("MKCOL")) {
            reply = new FakeMkcolReply { info, op, newRequest, this };
        } else if (verb == QLatin1String("DELETE") || op == QNetworkAccessManager::DeleteOperation) {
            reply = new FakeDeleteReply { info, op, newRequest, this };
        } else if (verb == QLatin1String("MOVE") && !isUpload) {
            reply = new FakeMoveReply { info, op, newRequest, this };
        } else if (verb == QLatin1String("MOVE") && isUpload) {
            reply = new FakeChunkMoveReply { info, _remoteRootFileInfo, op, newRequest, this };
        } else if (verb == QLatin1String("POST") || op == QNetworkAccessManager::PostOperation) {
            if (contentType.startsWith(QStringLiteral("multipart/related; boundary="))) {
                reply = new FakePutMultiFileReply { info, op, newRequest, contentType, outgoingData->readAll(), _serverVersion, this };
            }
        } else if (verb == QLatin1String("LOCK") || verb == QLatin1String("UNLOCK")) {
            reply = new FakeFileLockReply{info, op, newRequest, this};
        } else {
            qDebug() << verb << outgoingData;
            Q_UNREACHABLE();
        }
    }
    OCC::HttpLogger::logRequest(reply, op, outgoingData);
    return reply;
}

QNetworkReply * FakeQNAM::overrideReplyWithError(QString fileName, QNetworkAccessManager::Operation op, QNetworkRequest newRequest)
{
    QNetworkReply *reply = nullptr;

    Q_ASSERT(!fileName.isNull());
    if (_errorPaths.contains(fileName)) {
        reply = new FakeErrorReply { op, newRequest, this, _errorPaths[fileName] };
    }

    return reply;
}

void FakeQNAM::setServerVersion(const QString &version)
{
    _serverVersion = version;
}

FakeFolder::FakeFolder(const FileInfo &fileTemplate, const OCC::Optional<FileInfo> &localFileInfo, const QString &remotePath)
    : _localModifier(_tempDir.path())
{
    // Needs to be done once
    OCC::SyncEngine::minimumFileAgeForUpload = std::chrono::milliseconds(0);
    OCC::Logger::instance()->setLogFile(QStringLiteral("-"));
    OCC::Logger::instance()->addLogRule({ QStringLiteral("sync.httplogger=true") });

    QDir rootDir { _tempDir.path() };
    qDebug() << "FakeFolder operating on" << rootDir;
    if (localFileInfo) {
        toDisk(rootDir, *localFileInfo);
    } else {
        toDisk(rootDir, fileTemplate);
    }

    _fakeQnam = new FakeQNAM(fileTemplate);
    _account = OCC::Account::create();
    _account->setUrl(QUrl(QStringLiteral("http://admin:admin@localhost/owncloud")));
    _account->setCredentials(new FakeCredentials { _fakeQnam });
    _account->setDavDisplayName(QStringLiteral("fakename"));
    _account->setServerVersion(_serverVersion);
    _fakeQnam->setServerVersion(_serverVersion);

    _journalDb = std::make_unique<OCC::SyncJournalDb>(localPath() + QStringLiteral(".sync_test.db"));
    _syncEngine = std::make_unique<OCC::SyncEngine>(_account, localPath(), OCC::SyncOptions{}, remotePath, _journalDb.get());
    // Ignore temporary files from the download. (This is in the default exclude list, but we don't load it)
    _syncEngine->excludedFiles().addManualExclude(QStringLiteral("]*.~*"));

    // handle aboutToRemoveAllFiles with a timeout in case our test does not handle it
    QObject::connect(_syncEngine.get(), &OCC::SyncEngine::aboutToRemoveAllFiles, _syncEngine.get(), [this](OCC::SyncFileItem::Direction, std::function<void(bool)> callback) {
        QTimer::singleShot(1 * 1000, _syncEngine.get(), [callback] {
            callback(false);
        });
    });

    // Ensure we have a valid VfsOff instance "running"
    switchToVfs(_syncEngine->syncOptions()._vfs);

    // A new folder will update the local file state database on first sync.
    // To have a state matching what users will encounter, we have to a sync
    // using an identical local/remote file tree first.
    ENFORCE(syncOnce());
}

void FakeFolder::switchToVfs(QSharedPointer<OCC::Vfs> vfs)
{
    auto opts = _syncEngine->syncOptions();

    opts._vfs->stop();
    QObject::disconnect(_syncEngine.get(), nullptr, opts._vfs.data(), nullptr);

    opts._vfs = vfs;
    _syncEngine->setSyncOptions(opts);

    OCC::VfsSetupParams vfsParams;
    vfsParams.filesystemPath = localPath();
    vfsParams.remotePath = QLatin1Char('/');
    vfsParams.account = _account;
    vfsParams.journal = _journalDb.get();
    vfsParams.providerName = QStringLiteral("OC-TEST");
    vfsParams.providerVersion = QStringLiteral("0.1");
    QObject::connect(_syncEngine.get(), &QObject::destroyed, vfs.data(), [vfs]() {
        vfs->stop();
        vfs->unregisterFolder();
    });

    vfs->start(vfsParams);
}

void FakeFolder::enableEnforceWindowsFileNameCompatibility()
{
    syncEngine().account()->setCapabilities(QVariantMap{
        {
            "files",
            QVariantMap{
                {
                    "forbidden_filename_basenames",
                    QStringList{"con",
                        "prn",
                        "aux",
                        "nul",
                        "com0",
                        "com1",
                        "com2",
                        "com3",
                        "com4",
                        "com5",
                        "com6",
                        "com7",
                        "com8",
                        "com9",
                        "com¹",
                        "com²",
                        "com³",
                        "lpt0",
                        "lpt1",
                        "lpt2",
                        "lpt3",
                        "lpt4",
                        "lpt5",
                        "lpt6",
                        "lpt7",
                        "lpt8",
                        "lpt9",
                        "lpt¹",
                        "lpt²",
                        "lpt³"
                    }
                },
                {
                    "forbidden_filename_characters",
                    QStringList{"\\",
                        "/",
                        "<",
                        ">",
                        ":",
                        "\"",
                        "|",
                        "?",
                        "*",
                        "\\",
                        "/"
                    }
                },
                {
                    "forbidden_filename_extensions",
                    QStringList{" ",
                        ".",
                        ".filepart",
                        ".part",
                        ".part"
                    }
                },
                {
                    "forbidden_filenames",
                    QStringList{"\\",
                        ".htaccess"
                    }
                }
            }
        }
    });
}

void FakeFolder::setServerVersion(const QString &version)
{
    if (_serverVersion == version) {
        return;
    }

    _serverVersion = version;
    _account->setServerVersion(_serverVersion);
    _fakeQnam->setServerVersion(_serverVersion);
}

FileInfo FakeFolder::currentLocalState()
{
    QDir rootDir { _tempDir.path() };
    FileInfo rootTemplate;
    fromDisk(rootDir, rootTemplate);
    rootTemplate.fixupParentPathRecursively();
    return rootTemplate;
}

QString FakeFolder::localPath() const
{
    // SyncEngine wants a trailing slash
    return OCC::Utility::trailingSlashPath(_tempDir.path());
}

void FakeFolder::scheduleSync()
{
    // Have to be done async, else, an error before exec() does not terminate the event loop.
    QMetaObject::invokeMethod(_syncEngine.get(), "startSync", Qt::QueuedConnection);
}

void FakeFolder::execUntilBeforePropagation()
{
    QSignalSpy spy(_syncEngine.get(), &OCC::SyncEngine::aboutToPropagate);
    QVERIFY(spy.wait());
}

void FakeFolder::execUntilItemCompleted(const QString &relativePath)
{
    QSignalSpy spy(_syncEngine.get(), &OCC::SyncEngine::itemCompleted);
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < 5000) {
        spy.clear();
        QVERIFY(spy.wait());
        for (const QList<QVariant> &args : spy) {
            auto item = args[0].value<OCC::SyncFileItemPtr>();
            if (item->destination() == relativePath)
                return;
        }
    }
    QVERIFY(false);
}

void FakeFolder::toDisk(QDir &dir, const FileInfo &templateFi)
{
    for(const auto &child : templateFi.children) {
        if (child.isDir) {
            QDir subDir(dir);
            dir.mkdir(child.name);
            subDir.cd(child.name);
            toDisk(subDir, child);
        } else {
            QFile file { dir.filePath(child.name) };
            file.open(QFile::WriteOnly);
            file.write(QByteArray {}.fill(child.contentChar, child.size));
            file.close();
            OCC::FileSystem::setModTime(file.fileName(), OCC::Utility::qDateTimeToTime_t(child.lastModified));
        }
    }
}

void FakeFolder::fromDisk(QDir &dir, FileInfo &templateFi)
{
    for(const auto &diskChild : dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot)) {
        if (diskChild.isDir()) {
            QDir subDir = dir;
            subDir.cd(diskChild.fileName());
            FileInfo &subFi = templateFi.children[diskChild.fileName()] = FileInfo { diskChild.fileName() };
            fromDisk(subDir, subFi);
        } else {
            QFile f { diskChild.filePath() };
            f.open(QFile::ReadOnly);
            auto content = f.read(1);
            if (content.size() == 0) {
                qWarning() << "Empty file at:" << diskChild.filePath();
                continue;
            }
            char contentChar = content.at(0);
            templateFi.children.insert(diskChild.fileName(), FileInfo{diskChild.fileName(), diskChild.size(), contentChar, diskChild.lastModified()});
        }
    }
}

static FileInfo &findOrCreateDirs(FileInfo &base, PathComponents components)
{
    if (components.isEmpty())
        return base;
    auto childName = components.pathRoot();
    auto it = base.children.find(childName);
    if (it != base.children.end()) {
        return findOrCreateDirs(*it, components.subComponents());
    }
    auto &newDir = base.children[childName] = FileInfo { childName };
    newDir.parentPath = base.path();
    return findOrCreateDirs(newDir, components.subComponents());
}

FileInfo FakeFolder::dbState() const
{
    FileInfo result;
    [[maybe_unused]] const auto journalDbResult =_journalDb->getFilesBelowPath("", [&](const OCC::SyncJournalFileRecord &record) {
        auto components = PathComponents(record.path());
        auto &parentDir = findOrCreateDirs(result, components.parentDirComponents());
        auto name = components.fileName();
        auto &item = parentDir.children[name];
        item.name = name;
        item.parentPath = parentDir.path();
        item.size = record._fileSize;
        item.isDir = record._type == ItemTypeDirectory;
        item.permissions = record._remotePerm;
        item.etag = record._etag;
        item.lastModified = OCC::Utility::qDateTimeFromTime_t(record._modtime);
        item.fileId = record._fileId;
        item.checksums = record._checksumHeader;
        // item.contentChar can't be set from the db
    });
    return result;
}

OCC::SyncFileItemPtr ItemCompletedSpy::findItem(const QString &path) const
{
    for (const QList<QVariant> &args : *this) {
        auto item = args[0].value<OCC::SyncFileItemPtr>();
        if (item->destination() == path)
            return item;
    }
    return OCC::SyncFileItemPtr::create();
}

OCC::SyncFileItemPtr ItemCompletedSpy::findItemWithExpectedRank(const QString &path, int rank) const
{
    Q_ASSERT(size() > rank);
    Q_ASSERT(!(*this)[rank].isEmpty());

    auto item = (*this)[rank][0].value<OCC::SyncFileItemPtr>();
    if (item->destination() == path) {
        return item;
    } else {
        return OCC::SyncFileItemPtr::create();
    }
}

FakeReply::FakeReply(QObject *parent)
    : QNetworkReply(parent)
{
    setRawHeader(QByteArrayLiteral("Date"), QDateTime::currentDateTimeUtc().toString(Qt::RFC2822Date).toUtf8());
}

FakeReply::~FakeReply() = default;

FakeJsonErrorReply::FakeJsonErrorReply(QNetworkAccessManager::Operation op,
                                       const QNetworkRequest &request,
                                       QObject *parent,
                                       int httpErrorCode,
                                       const QJsonDocument &reply)
    : FakeErrorReply{ op, request, parent, httpErrorCode, reply.toJson() }
{
}

FakeFileLockReply::FakeFileLockReply(FileInfo &remoteRootFileInfo,
                                     QNetworkAccessManager::Operation op,
                                     const QNetworkRequest &request,
                                     QObject *parent)
    : FakePropfindReply(remoteRootFileInfo, op, request, parent)
{
    const auto verb = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();

    setRequest(request);
    setUrl(request.url());
    setOperation(op);
    open(QIODevice::ReadOnly);

    QString fileName = getFilePathFromUrl(request.url());
    Q_ASSERT(!fileName.isNull()); // for root, it should be empty
    FileInfo *fileInfo = remoteRootFileInfo.find(fileName);
    if (!fileInfo) {
        QMetaObject::invokeMethod(this, "respond404", Qt::QueuedConnection);
        return;
    }

    const QString prefix = request.url().path().left(request.url().path().size() - fileName.size());

    // Don't care about the request and just return a full propfind
    const QString davUri { QStringLiteral("DAV:") };
    const QString ocUri { QStringLiteral("http://owncloud.org/ns") };
    const QString ncUri { QStringLiteral("http://nextcloud.org/ns") };
    payload.clear();
    QBuffer buffer { &payload };
    buffer.open(QIODevice::WriteOnly);
    QXmlStreamWriter xml(&buffer);
    xml.writeNamespace(davUri, QStringLiteral("d"));
    xml.writeNamespace(ocUri, QStringLiteral("oc"));
    xml.writeNamespace(ncUri, QStringLiteral("nc"));
    xml.writeStartDocument();
    xml.writeStartElement(davUri, QStringLiteral("prop"));
    xml.writeTextElement(ncUri, QStringLiteral("lock"), verb == QStringLiteral("LOCK") ? "1" : "0");
    xml.writeTextElement(ncUri, QStringLiteral("lock-owner-type"), QString::number(0));
    xml.writeTextElement(ncUri, QStringLiteral("lock-owner"), QStringLiteral("admin"));
    xml.writeTextElement(ncUri, QStringLiteral("lock-owner-displayname"), QStringLiteral("John Doe"));
    xml.writeTextElement(ncUri, QStringLiteral("lock-owner-editor"), {});
    xml.writeTextElement(ncUri, QStringLiteral("lock-time"), QString::number(1234560));
    xml.writeTextElement(ncUri, QStringLiteral("lock-timeout"), QString::number(1800));
    xml.writeEndElement(); // prop
    xml.writeEndDocument();
}

FakeJsonReply::FakeJsonReply(QNetworkAccessManager::Operation op,
                             const QNetworkRequest &request,
                             QObject *parent,
                             int httpReturnCode,
                             const QJsonDocument &reply)
    : FakeReply{parent}
    , _body{reply.toJson()}
{
    setRequest(request);
    setUrl(request.url());
    setOperation(op);
    open(QIODevice::ReadOnly);
    setAttribute(QNetworkRequest::HttpStatusCodeAttribute, httpReturnCode);
    QMetaObject::invokeMethod(this, &FakeJsonReply::respond, Qt::QueuedConnection);
}

void FakeJsonReply::respond()
{
    emit metaDataChanged();
    emit readyRead();
    // finishing can come strictly after readyRead was called
    QTimer::singleShot(5, this, &FakeJsonReply::slotSetFinished);
}

void FakeJsonReply::slotSetFinished()
{
    setFinished(true);
    emit finished();
}

qint64 FakeJsonReply::readData(char *buf, qint64 max)
{
    max = qMin<qint64>(max, _body.size());
    memcpy(buf, _body.constData(), max);
    _body = _body.mid(max);
    return max;
}

qint64 FakeJsonReply::bytesAvailable() const
{
    return _body.size();
}
