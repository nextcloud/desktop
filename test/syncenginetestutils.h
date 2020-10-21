/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */
#pragma once

#include "account.h"
#include "creds/abstractcredentials.h"
#include "logger.h"
#include "filesystem.h"
#include "syncengine.h"
#include "common/syncjournaldb.h"
#include "common/syncjournalfilerecord.h"
#include "common/vfs.h"
#include "csync_exclude.h"
#include <cstring>

#include <QDir>
#include <QNetworkReply>
#include <QMap>
#include <QtTest>
#include <cookiejar.h>
#include <QTimer>

/*
 * TODO: In theory we should use QVERIFY instead of Q_ASSERT for testing, but this
 * only works when directly called from a QTest :-(
 */


static const QUrl sRootUrl = QUrl::fromEncoded("owncloud://somehost/owncloud/remote.php/webdav/");
static const QUrl sRootUrl2 = QUrl::fromEncoded("owncloud://somehost/owncloud/remote.php/dav/files/admin/");
static const QUrl sUploadUrl = QUrl::fromEncoded("owncloud://somehost/owncloud/remote.php/dav/uploads/admin/");

inline QString getFilePathFromUrl(const QUrl &url) {
    QString path = url.path();
    if (path.startsWith(sRootUrl.path()))
        return path.mid(sRootUrl.path().length());
    if (path.startsWith(sRootUrl2.path()))
        return path.mid(sRootUrl2.path().length());
    if (path.startsWith(sUploadUrl.path()))
        return path.mid(sUploadUrl.path().length());
    return {};
}


inline QByteArray generateEtag() {
    return QByteArray::number(QDateTime::currentDateTimeUtc().toMSecsSinceEpoch(), 16) + QByteArray::number(qrand(), 16);
}
inline QByteArray generateFileId() {
    return QByteArray::number(qrand(), 16);
}

class PathComponents : public QStringList {
public:
    PathComponents(const char *path) : PathComponents{QString::fromUtf8(path)} {}
    PathComponents(const QString &path) : QStringList{path.split(QLatin1Char('/'), QString::SkipEmptyParts)} { }
    PathComponents(const QStringList &pathComponents) : QStringList{pathComponents} { }

    PathComponents parentDirComponents() const {
        return PathComponents{mid(0, size() - 1)};
    }
    PathComponents subComponents() const& { return PathComponents{mid(1)}; }
    PathComponents subComponents() && { removeFirst(); return std::move(*this); }
    QString pathRoot() const { return first(); }
    QString fileName() const { return last(); }
};

class FileModifier
{
public:
    virtual ~FileModifier() { }
    virtual void remove(const QString &relativePath) = 0;
    virtual void insert(const QString &relativePath, qint64 size = 64, char contentChar = 'W') = 0;
    virtual void setContents(const QString &relativePath, char contentChar) = 0;
    virtual void appendByte(const QString &relativePath, char contentChar = 0) = 0;
    virtual void modifyByte(const QString &relativePath, quint64 offset, char contentChar) = 0;
    virtual void mkdir(const QString &relativePath) = 0;
    virtual void rename(const QString &relativePath, const QString &relativeDestinationDirectory) = 0;
    virtual void setModTime(const QString &relativePath, const QDateTime &modTime) = 0;
};

class DiskFileModifier : public FileModifier
{
    QDir _rootDir;
public:
    DiskFileModifier(const QString &rootDirPath) : _rootDir(rootDirPath) { }
    void remove(const QString &relativePath) override {
        QFileInfo fi{_rootDir.filePath(relativePath)};
        if (fi.isFile())
            QVERIFY(_rootDir.remove(relativePath));
        else
            QVERIFY(QDir{fi.filePath()}.removeRecursively());
    }
    void insert(const QString &relativePath, qint64 size = 64, char contentChar = 'W') override {
        QFile file{_rootDir.filePath(relativePath)};
        QVERIFY(!file.exists());
        file.open(QFile::WriteOnly);
        QByteArray buf(1024, contentChar);
        for (int x = 0; x < size/buf.size(); ++x) {
            file.write(buf);
        }
        file.write(buf.data(), size % buf.size());
        file.close();
        // Set the mtime 30 seconds in the past, for some tests that need to make sure that the mtime differs.
        OCC::FileSystem::setModTime(file.fileName(), OCC::Utility::qDateTimeToTime_t(QDateTime::currentDateTimeUtc().addSecs(-30)));
        QCOMPARE(file.size(), size);
    }
    void setContents(const QString &relativePath, char contentChar) override {
        QFile file{_rootDir.filePath(relativePath)};
        QVERIFY(file.exists());
        qint64 size = file.size();
        file.open(QFile::WriteOnly);
        file.write(QByteArray{}.fill(contentChar, size));
    }
    void appendByte(const QString &relativePath, char contentChar) override
    {
        QFile file{_rootDir.filePath(relativePath)};
        QVERIFY(file.exists());
        file.open(QFile::ReadWrite);
        QByteArray contents;
        if (contentChar)
            contents += contentChar;
        else
            contents = file.read(1);
        file.seek(file.size());
        file.write(contents);
    }
    void modifyByte(const QString &relativePath, quint64 offset, char contentChar) override
    {
        QFile file{ _rootDir.filePath(relativePath) };
        QVERIFY(file.exists());
        file.open(QFile::ReadWrite);
        file.seek(offset);
        file.write(&contentChar, 1);
        file.close();
    }

    void mkdir(const QString &relativePath) override {
        _rootDir.mkpath(relativePath);
    }
    void rename(const QString &from, const QString &to) override {
        QVERIFY(_rootDir.exists(from));
        QVERIFY(_rootDir.rename(from, to));
    }
    void setModTime(const QString &relativePath, const QDateTime &modTime) override {
        OCC::FileSystem::setModTime(_rootDir.filePath(relativePath), OCC::Utility::qDateTimeToTime_t(modTime));
    }
};

class FileInfo : public FileModifier
{
public:
    static FileInfo A12_B12_C12_S12() {
        FileInfo fi{QString{}, {
            {QStringLiteral("A"), {
                {QStringLiteral("a1"), 4},
                {QStringLiteral("a2"), 4}
            }},
            {QStringLiteral("B"), {
                {QStringLiteral("b1"), 16},
                {QStringLiteral("b2"), 16}
            }},
            {QStringLiteral("C"), {
                {QStringLiteral("c1"), 24},
                {QStringLiteral("c2"), 24}
            }},
        }};
        FileInfo sharedFolder{QStringLiteral("S"), {
            {QStringLiteral("s1"), 32},
            {QStringLiteral("s2"), 32}
        }};
        sharedFolder.isShared = true;
        sharedFolder.children[QStringLiteral("s1")].isShared = true;
        sharedFolder.children[QStringLiteral("s2")].isShared = true;
        fi.children.insert(sharedFolder.name, std::move(sharedFolder));
        return fi;
    }

    FileInfo() = default;
    FileInfo(const QString &name) : name{name} { }
    FileInfo(const QString &name, qint64 size) : name{name}, isDir{false}, size{size} { }
    FileInfo(const QString &name, qint64 size, char contentChar) : name{name}, isDir{false}, size{size}, contentChar{contentChar} { }
    FileInfo(const QString &name, const std::initializer_list<FileInfo> &children) : name{name} {
        for (const auto &source : children)
            addChild(source);
    }

    void addChild(const FileInfo &info)
    {
        auto &dest = this->children[info.name] = info;
        dest.parentPath = path();
        dest.fixupParentPathRecursively();
    }

    void remove(const QString &relativePath) override {
        const PathComponents pathComponents{relativePath};
        FileInfo *parent = findInvalidatingEtags(pathComponents.parentDirComponents());
        Q_ASSERT(parent);
        parent->children.erase(std::find_if(parent->children.begin(), parent->children.end(),
                [&pathComponents](const FileInfo &fi){ return fi.name == pathComponents.fileName(); }));
    }

    void insert(const QString &relativePath, qint64 size = 64, char contentChar = 'W') override {
        create(relativePath, size, contentChar);
    }

    void setContents(const QString &relativePath, char contentChar) override {
        FileInfo *file = findInvalidatingEtags(relativePath);
        Q_ASSERT(file);
        file->contentChar = contentChar;
    }

    void appendByte(const QString &relativePath, char contentChar = 0) override
    {
        Q_UNUSED(contentChar);
        FileInfo *file = findInvalidatingEtags(relativePath);
        Q_ASSERT(file);
        file->size += 1;
    }

    void modifyByte(const QString &relativePath, quint64 offset, char contentChar) override
    {
        Q_UNUSED(offset);
        Q_UNUSED(contentChar);
        FileInfo *file = findInvalidatingEtags(relativePath);
        Q_ASSERT(file);
        Q_ASSERT(!"unimplemented");
    }

    void mkdir(const QString &relativePath) override {
        createDir(relativePath);
    }

    void rename(const QString &oldPath, const QString &newPath) override {
        const PathComponents newPathComponents{newPath};
        FileInfo *dir = findInvalidatingEtags(newPathComponents.parentDirComponents());
        Q_ASSERT(dir);
        Q_ASSERT(dir->isDir);
        const PathComponents pathComponents{oldPath};
        FileInfo *parent = findInvalidatingEtags(pathComponents.parentDirComponents());
        Q_ASSERT(parent);
        FileInfo fi = parent->children.take(pathComponents.fileName());
        fi.parentPath = dir->path();
        fi.name = newPathComponents.fileName();
        fi.fixupParentPathRecursively();
        dir->children.insert(newPathComponents.fileName(), std::move(fi));
    }

    void setModTime(const QString &relativePath, const QDateTime &modTime) override {
        FileInfo *file = findInvalidatingEtags(relativePath);
        Q_ASSERT(file);
        file->lastModified = modTime;
    }

    FileInfo *find(PathComponents pathComponents, const bool invalidateEtags = false) {
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

    FileInfo *createDir(const QString &relativePath) {
        const PathComponents pathComponents{relativePath};
        FileInfo *parent = findInvalidatingEtags(pathComponents.parentDirComponents());
        Q_ASSERT(parent);
        FileInfo &child = parent->children[pathComponents.fileName()] = FileInfo{pathComponents.fileName()};
        child.parentPath = parent->path();
        child.etag = generateEtag();
        return &child;
    }

    FileInfo *create(const QString &relativePath, qint64 size, char contentChar) {
        const PathComponents pathComponents{relativePath};
        FileInfo *parent = findInvalidatingEtags(pathComponents.parentDirComponents());
        Q_ASSERT(parent);
        FileInfo &child = parent->children[pathComponents.fileName()] = FileInfo{pathComponents.fileName(), size};
        child.parentPath = parent->path();
        child.contentChar = contentChar;
        child.etag = generateEtag();
        return &child;
    }

    bool operator<(const FileInfo &other) const {
        return name < other.name;
    }

    bool operator==(const FileInfo &other) const {
        // Consider files to be equal between local<->remote as a user would.
        return name == other.name
            && isDir == other.isDir
            && size == other.size
            && contentChar == other.contentChar
            && children == other.children;
    }

    bool operator!=(const FileInfo &other) const {
        return !operator==(other);
    }

    QString path() const {
        return (parentPath.isEmpty() ? QString() : (parentPath + QLatin1Char('/'))) + name;
    }

    void fixupParentPathRecursively() {
        auto p = path();
        for (auto it = children.begin(); it != children.end(); ++it) {
            Q_ASSERT(it.key() == it->name);
            it->parentPath = p;
            it->fixupParentPathRecursively();
        }
    }

    QString name;
    bool isDir = true;
    bool isShared = false;
    OCC::RemotePermissions permissions; // When uset, defaults to everything
    QDateTime lastModified = QDateTime::currentDateTimeUtc().addDays(-7);
    QByteArray etag = generateEtag();
    QByteArray fileId = generateFileId();
    QByteArray checksums;
    QByteArray extraDavProperties;
    qint64 size = 0;
    char contentChar = 'W';

    // Sorted by name to be able to compare trees
    QMap<QString, FileInfo> children;
    QString parentPath;

    FileInfo *findInvalidatingEtags(PathComponents pathComponents) {
        return find(std::move(pathComponents), true);
    }

    friend inline QDebug operator<<(QDebug dbg, const FileInfo& fi) {
        return dbg << "{ " << fi.path() << ": " << fi.children;
    }
};

class FakePropfindReply : public QNetworkReply
{
    Q_OBJECT
public:
    QByteArray payload;

    FakePropfindReply(FileInfo &remoteRootFileInfo, QNetworkAccessManager::Operation op, const QNetworkRequest &request, QObject *parent)
    : QNetworkReply{parent} {
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
        QString prefix = request.url().path().left(request.url().path().size() - fileName.size());

        // Don't care about the request and just return a full propfind
        const QString davUri{QStringLiteral("DAV:")};
        const QString ocUri{QStringLiteral("http://owncloud.org/ns")};
        QBuffer buffer{&payload};
        buffer.open(QIODevice::WriteOnly);
        QXmlStreamWriter xml( &buffer );
        xml.writeNamespace(davUri, QStringLiteral("d"));
        xml.writeNamespace(ocUri, QStringLiteral("oc"));
        xml.writeStartDocument();
        xml.writeStartElement(davUri, QStringLiteral("multistatus"));
        auto writeFileResponse = [&](const FileInfo &fileInfo) {
            xml.writeStartElement(davUri, QStringLiteral("response"));

            xml.writeTextElement(davUri, QStringLiteral("href"), prefix + QString::fromUtf8(QUrl::toPercentEncoding(fileInfo.path(), "/")));
            xml.writeStartElement(davUri, QStringLiteral("propstat"));
            xml.writeStartElement(davUri, QStringLiteral("prop"));

            if (fileInfo.isDir) {
                xml.writeStartElement(davUri, QStringLiteral("resourcetype"));
                xml.writeEmptyElement(davUri, QStringLiteral("collection"));
                xml.writeEndElement(); // resourcetype
            } else
                xml.writeEmptyElement(davUri, QStringLiteral("resourcetype"));

            auto gmtDate = fileInfo.lastModified.toUTC();
            auto stringDate = QLocale::c().toString(gmtDate, QStringLiteral("ddd, dd MMM yyyy HH:mm:ss 'GMT'"));
            xml.writeTextElement(davUri, QStringLiteral("getlastmodified"), stringDate);
            xml.writeTextElement(davUri, QStringLiteral("getcontentlength"), QString::number(fileInfo.size));
            xml.writeTextElement(davUri, QStringLiteral("getetag"), QStringLiteral("\"%1\"").arg(QString::fromLatin1(fileInfo.etag)));
            xml.writeTextElement(ocUri, QStringLiteral("permissions"), !fileInfo.permissions.isNull()
                ? QString(fileInfo.permissions.toString())
                : fileInfo.isShared ? QStringLiteral("SRDNVCKW") : QStringLiteral("RDNVCKW"));
            xml.writeTextElement(ocUri, QStringLiteral("id"), QString::fromUtf8(fileInfo.fileId));
            xml.writeTextElement(ocUri, QStringLiteral("checksums"), QString::fromUtf8(fileInfo.checksums));
            buffer.write(fileInfo.extraDavProperties);
            xml.writeEndElement(); // prop
            xml.writeTextElement(davUri, QStringLiteral("status"), QStringLiteral("HTTP/1.1 200 OK"));
            xml.writeEndElement(); // propstat
            xml.writeEndElement(); // response
        };

        writeFileResponse(*fileInfo);
        foreach(const FileInfo &childFileInfo, fileInfo->children)
           writeFileResponse(childFileInfo);
        xml.writeEndElement(); // multistatus
        xml.writeEndDocument();

        QMetaObject::invokeMethod(this, "respond", Qt::QueuedConnection);
    }

    Q_INVOKABLE void respond() {
        setHeader(QNetworkRequest::ContentLengthHeader, payload.size());
        setHeader(QNetworkRequest::ContentTypeHeader, QByteArrayLiteral("application/xml; charset=utf-8"));
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 207);
        setFinished(true);
        emit metaDataChanged();
        if (bytesAvailable())
            emit readyRead();
        emit finished();
    }

    Q_INVOKABLE void respond404() {
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 404);
        setError(InternalServerError, QStringLiteral("Not Found"));
        emit metaDataChanged();
        emit finished();
    }

    void abort() override { }

    qint64 bytesAvailable() const override { return payload.size() + QIODevice::bytesAvailable(); }
    qint64 readData(char *data, qint64 maxlen) override {
        qint64 len = std::min(qint64{payload.size()}, maxlen);
        std::copy(payload.cbegin(), payload.cbegin() + len, data);
        payload.remove(0, static_cast<int>(len));
        return len;
    }
};

class FakePutReply : public QNetworkReply
{
    Q_OBJECT
    FileInfo *fileInfo;
public:
    FakePutReply(FileInfo &remoteRootFileInfo, QNetworkAccessManager::Operation op, const QNetworkRequest &request, const QByteArray &putPayload, QObject *parent)
    : QNetworkReply{parent} {
        setRequest(request);
        setUrl(request.url());
        setOperation(op);
        open(QIODevice::ReadOnly);
        fileInfo = perform(remoteRootFileInfo, request, putPayload);
        QMetaObject::invokeMethod(this, "respond", Qt::QueuedConnection);
    }

    static FileInfo *perform(FileInfo &remoteRootFileInfo, const QNetworkRequest &request, const QByteArray &putPayload)
    {
        QString fileName = getFilePathFromUrl(request.url());
        Q_ASSERT(!fileName.isEmpty());
        FileInfo *fileInfo = remoteRootFileInfo.find(fileName);
        if (fileInfo) {
            fileInfo->size = putPayload.size();
            fileInfo->contentChar = putPayload.at(0);
        } else {
            // Assume that the file is filled with the same character
            fileInfo = remoteRootFileInfo.create(fileName, putPayload.size(), putPayload.at(0));
        }
        fileInfo->lastModified = OCC::Utility::qDateTimeFromTime_t(request.rawHeader("X-OC-Mtime").toLongLong());
        remoteRootFileInfo.find(fileName, /*invalidate_etags=*/true);
        return fileInfo;
    }

    Q_INVOKABLE virtual void respond()
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

    void abort() override
    {
        setError(OperationCanceledError, QStringLiteral("abort"));
        emit finished();
    }
    qint64 readData(char *, qint64) override { return 0; }
};

class FakeMkcolReply : public QNetworkReply
{
    Q_OBJECT
    FileInfo *fileInfo;
public:
    FakeMkcolReply(FileInfo &remoteRootFileInfo, QNetworkAccessManager::Operation op, const QNetworkRequest &request, QObject *parent)
    : QNetworkReply{parent} {
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

    Q_INVOKABLE void respond() {
        setRawHeader("OC-FileId", fileInfo->fileId);
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 201);
        emit metaDataChanged();
        emit finished();
    }

    void abort() override { }
    qint64 readData(char *, qint64) override { return 0; }
};

class FakeDeleteReply : public QNetworkReply
{
    Q_OBJECT
public:
    FakeDeleteReply(FileInfo &remoteRootFileInfo, QNetworkAccessManager::Operation op, const QNetworkRequest &request, QObject *parent)
    : QNetworkReply{parent} {
        setRequest(request);
        setUrl(request.url());
        setOperation(op);
        open(QIODevice::ReadOnly);

        QString fileName = getFilePathFromUrl(request.url());
        Q_ASSERT(!fileName.isEmpty());
        remoteRootFileInfo.remove(fileName);
        QMetaObject::invokeMethod(this, "respond", Qt::QueuedConnection);
    }

    Q_INVOKABLE void respond() {
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 204);
        emit metaDataChanged();
        emit finished();
    }

    void abort() override { }
    qint64 readData(char *, qint64) override { return 0; }
};

class FakeMoveReply : public QNetworkReply
{
    Q_OBJECT
public:
    FakeMoveReply(FileInfo &remoteRootFileInfo, QNetworkAccessManager::Operation op, const QNetworkRequest &request, QObject *parent)
    : QNetworkReply{parent} {
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

    Q_INVOKABLE void respond() {
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 201);
        emit metaDataChanged();
        emit finished();
    }

    void abort() override { }
    qint64 readData(char *, qint64) override { return 0; }
};

class FakeGetReply : public QNetworkReply
{
    Q_OBJECT
public:
    const FileInfo *fileInfo;
    char payload;
    int size;
    bool aborted = false;

    FakeGetReply(FileInfo &remoteRootFileInfo, QNetworkAccessManager::Operation op, const QNetworkRequest &request, QObject *parent)
    : QNetworkReply{parent} {
        setRequest(request);
        setUrl(request.url());
        setOperation(op);
        open(QIODevice::ReadOnly);

        QString fileName = getFilePathFromUrl(request.url());
        Q_ASSERT(!fileName.isEmpty());
        fileInfo = remoteRootFileInfo.find(fileName);
        if (!fileInfo)
            qWarning() << "Could not find file" << fileName << "on the remote";
        QMetaObject::invokeMethod(this, "respond", Qt::QueuedConnection);
    }

    Q_INVOKABLE void respond() {
        if (aborted) {
            setError(OperationCanceledError, QStringLiteral("Operation Canceled"));
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

    void abort() override {
        setError(OperationCanceledError, QStringLiteral("Operation Canceled"));
        aborted = true;
    }
    qint64 bytesAvailable() const override {
        if (aborted)
            return 0;
        return size + QIODevice::bytesAvailable();
    }

    qint64 readData(char *data, qint64 maxlen) override {
        qint64 len = std::min(qint64{size}, maxlen);
        std::fill_n(data, len, payload);
        size -= len;
        return len;
    }

    // useful to be public for testing
    using QNetworkReply::setRawHeader;
};

class FakeGetWithDataReply : public QNetworkReply
{
    Q_OBJECT
public:
    const FileInfo *fileInfo;
    QByteArray payload;
    quint64 offset = 0;
    bool aborted = false;

    FakeGetWithDataReply(FileInfo &remoteRootFileInfo, const QByteArray &data, QNetworkAccessManager::Operation op, const QNetworkRequest &request, QObject *parent)
        : QNetworkReply{ parent }
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
            if (match.hasMatch())
            {
                const int start = match.captured(QStringLiteral("start")).toInt();
                const int end = match.captured(QStringLiteral("end")).toInt();
                payload = payload.mid(start, end - start + 1);
            }
        }
    }

    Q_INVOKABLE void respond()
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

    void abort() override
    {
        setError(OperationCanceledError, QStringLiteral("Operation Canceled"));
        aborted = true;
    }
    qint64 bytesAvailable() const override
    {
        if (aborted)
            return 0;
        return payload.size() - offset + QIODevice::bytesAvailable();
    }

    qint64 readData(char *data, qint64 maxlen) override
    {
        qint64 len = std::min(payload.size() - offset, quint64(maxlen));
        std::memcpy(data, payload.constData() + offset, len);
        offset += len;
        return len;
    }

    // useful to be public for testing
    using QNetworkReply::setRawHeader;
};

class FakeChunkMoveReply : public QNetworkReply
{
    Q_OBJECT
    FileInfo *fileInfo;
public:
    FakeChunkMoveReply(FileInfo &uploadsFileInfo, FileInfo &remoteRootFileInfo,
        QNetworkAccessManager::Operation op, const QNetworkRequest &request,
        QObject *parent)
        : QNetworkReply{ parent }
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

    static FileInfo *perform(FileInfo &uploadsFileInfo, FileInfo &remoteRootFileInfo, const QNetworkRequest &request)
    {
        QString source = getFilePathFromUrl(request.url());
        Q_ASSERT(!source.isEmpty());
        Q_ASSERT(source.endsWith(QLatin1String("/.file")));
        source = source.left(source.length() - qstrlen("/.file"));

        auto sourceFolder = uploadsFileInfo.find(source);
        Q_ASSERT(sourceFolder);
        Q_ASSERT(sourceFolder->isDir);
        int count = 0;
        qlonglong size = 0;
        qlonglong prev = 0;
        char payload = '\0';

        QString fileName = getFilePathFromUrl(QUrl::fromEncoded(request.rawHeader("Destination")));
        Q_ASSERT(!fileName.isEmpty());

        // Compute the size and content from the chunks if possible
        for (auto chunkName : sourceFolder->children.keys()) {
            auto &x = sourceFolder->children[chunkName];
            if (chunkName.toLongLong() != prev)
                break;
            Q_ASSERT(!x.isDir);
            Q_ASSERT(x.size > 0); // There should not be empty chunks
            size += x.size;
            Q_ASSERT(!payload || payload == x.contentChar);
            payload = x.contentChar;
            ++count;
            prev = chunkName.toLongLong() + x.size;
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
        remoteRootFileInfo.find(fileName, /*invalidate_etags=*/true);

        return fileInfo;
    }

    Q_INVOKABLE virtual void respond()
    {
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 201);
        setRawHeader("OC-ETag", fileInfo->etag);
        setRawHeader("ETag", fileInfo->etag);
        setRawHeader("OC-FileId", fileInfo->fileId);
        emit metaDataChanged();
        emit finished();
    }

    Q_INVOKABLE void respondPreconditionFailed() {
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 412);
        setError(InternalServerError, QStringLiteral("Precondition Failed"));
        emit metaDataChanged();
        emit finished();
    }

    void abort() override
    {
        setError(OperationCanceledError, QStringLiteral("abort"));
        emit finished();
    }

    qint64 readData(char *, qint64) override { return 0; }
};

class FakePayloadReply : public QNetworkReply
{
    Q_OBJECT
public:
    FakePayloadReply(QNetworkAccessManager::Operation op, const QNetworkRequest &request,
        const QByteArray &body, QObject *parent)
        : QNetworkReply{ parent }
        , _body(body)
    {
        setRequest(request);
        setUrl(request.url());
        setOperation(op);
        open(QIODevice::ReadOnly);
        QTimer::singleShot(10, this, &FakePayloadReply::respond);
    }

    void respond()
    {
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);
        setHeader(QNetworkRequest::ContentLengthHeader, _body.size());
        emit metaDataChanged();
        emit readyRead();
        setFinished(true);
        emit finished();
    }

    void abort() override {}
    qint64 readData(char *buf, qint64 max) override
    {
        max = qMin<qint64>(max, _body.size());
        memcpy(buf, _body.constData(), max);
        _body = _body.mid(max);
        return max;
    }
    qint64 bytesAvailable() const override
    {
        return _body.size();
    }
    QByteArray _body;
};


class FakeErrorReply : public QNetworkReply
{
    Q_OBJECT
public:
    FakeErrorReply(QNetworkAccessManager::Operation op, const QNetworkRequest &request,
                   QObject *parent, int httpErrorCode, const QByteArray &body = QByteArray())
    : QNetworkReply{parent}, _body(body) {
        setRequest(request);
        setUrl(request.url());
        setOperation(op);
        open(QIODevice::ReadOnly);
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, httpErrorCode);
        setError(InternalServerError, QStringLiteral("Internal Server Fake Error"));
        QMetaObject::invokeMethod(this, "respond", Qt::QueuedConnection);
    }

    Q_INVOKABLE virtual void respond() {
        emit metaDataChanged();
        emit readyRead();
        // finishing can come strictly after readyRead was called
        QTimer::singleShot(5, this, &FakeErrorReply::slotSetFinished);
    }

    // make public to give tests easy interface
    using QNetworkReply::setError;
    using QNetworkReply::setAttribute;

public slots:
    void slotSetFinished() {
        setFinished(true);
        emit finished();
    }

public:
    void abort() override { }
    qint64 readData(char *buf, qint64 max) override {
        max = qMin<qint64>(max, _body.size());
        memcpy(buf, _body.constData(), max);
        _body = _body.mid(max);
        return max;
    }
    qint64 bytesAvailable() const override {
        return _body.size();
    }

    QByteArray _body;
};

// A reply that never responds
class FakeHangingReply : public QNetworkReply
{
    Q_OBJECT
public:
    FakeHangingReply(QNetworkAccessManager::Operation op, const QNetworkRequest &request, QObject *parent)
        : QNetworkReply(parent)
    {
        setRequest(request);
        setUrl(request.url());
        setOperation(op);
        open(QIODevice::ReadOnly);
    }

    void abort() override {
        // Follow more or less the implementation of QNetworkReplyImpl::abort
        close();
        setError(OperationCanceledError, tr("Operation canceled"));
        emit error(OperationCanceledError);
        setFinished(true);
        emit finished();
    }
    qint64 readData(char *, qint64) override { return 0; }
};

// A delayed reply
template <class OriginalReply>
class DelayedReply : public OriginalReply
{
public:
    template <typename... Args>
    explicit DelayedReply(quint64 delayMS, Args &&... args)
        : OriginalReply(std::forward<Args>(args)...)
        , _delayMs(delayMS)
    {
    }
    quint64 _delayMs;

    void respond() override
    {
        QTimer::singleShot(_delayMs, static_cast<OriginalReply *>(this), [this] {
            // Explicit call to bases's respond();
            this->OriginalReply::respond();
        });
    }
};

class FakeQNAM : public QNetworkAccessManager
{
public:
    using Override = std::function<QNetworkReply *(Operation, const QNetworkRequest &, QIODevice *)>;

private:
    FileInfo _remoteRootFileInfo;
    FileInfo _uploadFileInfo;
    // maps a path to an HTTP error
    QHash<QString, int> _errorPaths;
    // monitor requests and optionally provide custom replies
    Override _override;

public:
    FakeQNAM(FileInfo initialRoot)
        : _remoteRootFileInfo{std::move(initialRoot)}
    {
        setCookieJar(new OCC::CookieJar);
    }
    FileInfo &currentRemoteState() { return _remoteRootFileInfo; }
    FileInfo &uploadState() { return _uploadFileInfo; }

    QHash<QString, int> &errorPaths() { return _errorPaths; }

    void setOverride(const Override &override) { _override = override; }

protected:
    QNetworkReply *createRequest(Operation op, const QNetworkRequest &request,
                                         QIODevice *outgoingData = nullptr) override {
        if (_override) {
            if (auto reply = _override(op, request, outgoingData))
                return reply;
        }
        const QString fileName = getFilePathFromUrl(request.url());
        Q_ASSERT(!fileName.isNull());
        if (_errorPaths.contains(fileName))
            return new FakeErrorReply{op, request, this, _errorPaths[fileName]};

        bool isUpload = request.url().path().startsWith(sUploadUrl.path());
        FileInfo &info = isUpload ? _uploadFileInfo : _remoteRootFileInfo;

        auto verb = request.attribute(QNetworkRequest::CustomVerbAttribute);
        if (verb == QLatin1String("PROPFIND"))
            // Ignore outgoingData always returning somethign good enough, works for now.
            return new FakePropfindReply{info, op, request, this};
        else if (verb == QLatin1String("GET") || op == QNetworkAccessManager::GetOperation)
            return new FakeGetReply{info, op, request, this};
        else if (verb == QLatin1String("PUT") || op == QNetworkAccessManager::PutOperation)
            return new FakePutReply{info, op, request, outgoingData->readAll(), this};
        else if (verb == QLatin1String("MKCOL"))
            return new FakeMkcolReply{info, op, request, this};
        else if (verb == QLatin1String("DELETE") || op == QNetworkAccessManager::DeleteOperation)
            return new FakeDeleteReply{info, op, request, this};
        else if (verb == QLatin1String("MOVE") && !isUpload)
            return new FakeMoveReply{info, op, request, this};
        else if (verb == QLatin1String("MOVE") && isUpload)
            return new FakeChunkMoveReply{ info, _remoteRootFileInfo, op, request, this };
        else {
            qDebug() << verb << outgoingData;
            Q_UNREACHABLE();
        }
    }
};

class FakeCredentials : public OCC::AbstractCredentials
{
    QNetworkAccessManager *_qnam;
public:
    FakeCredentials(QNetworkAccessManager *qnam) : _qnam{qnam} { }
    QString authType() const override { return QStringLiteral("test"); }
    QString user() const override { return QStringLiteral("admin"); }
    QNetworkAccessManager *createQNAM() const override { return _qnam; }
    bool ready() const override { return true; }
    void fetchFromKeychain() override { }
    void askFromUser() override { }
    bool stillValid(QNetworkReply *) override { return true; }
    void persist() override { }
    void invalidateToken() override { }
    void forgetSensitiveData() override { }
};

class FakeFolder
{
    QTemporaryDir _tempDir;
    DiskFileModifier _localModifier;
    // FIXME: Clarify ownership, double delete
    FakeQNAM *_fakeQnam;
    OCC::AccountPtr _account;
    std::unique_ptr<OCC::SyncJournalDb> _journalDb;
    std::unique_ptr<OCC::SyncEngine> _syncEngine;

public:
    FakeFolder(const FileInfo &fileTemplate)
        : _localModifier(_tempDir.path())
    {
        // Needs to be done once
        OCC::SyncEngine::minimumFileAgeForUpload = std::chrono::milliseconds(0);
        OCC::Logger::instance()->setLogFile(QStringLiteral("-"));

        QDir rootDir{_tempDir.path()};
        qDebug() << "FakeFolder operating on" << rootDir;
        toDisk(rootDir, fileTemplate);

        _fakeQnam = new FakeQNAM(fileTemplate);
        _account = OCC::Account::create();
        _account->setUrl(QUrl(QStringLiteral("http://admin:admin@localhost/owncloud")));
        _account->setCredentials(new FakeCredentials{_fakeQnam});
        _account->setDavDisplayName(QStringLiteral("fakename"));
        _account->setServerVersion(QStringLiteral("10.0.0"));

        _journalDb.reset(new OCC::SyncJournalDb(localPath() + QStringLiteral(".sync_test.db")));
        _syncEngine.reset(new OCC::SyncEngine(_account, localPath(), QString(), _journalDb.get()));
        // Ignore temporary files from the download. (This is in the default exclude list, but we don't load it)
        _syncEngine->excludedFiles().addManualExclude(QStringLiteral("]*.~*"));

        // handle aboutToRemoveAllFiles with a timeout in case our test does not handle it
        QObject::connect(_syncEngine.get(), &OCC::SyncEngine::aboutToRemoveAllFiles, _syncEngine.get(), [this](OCC::SyncFileItem::Direction, std::function<void(bool)> callback){
            QTimer::singleShot(1 * 1000, _syncEngine.get(), [callback]{
                callback(false);
            });
        });

        // Ensure we have a valid VfsOff instance "running"
        switchToVfs(_syncEngine->syncOptions()._vfs);

        // A new folder will update the local file state database on first sync.
        // To have a state matching what users will encounter, we have to a sync
        // using an identical local/remote file tree first.
        OC_ENFORCE(syncOnce());
    }

    void switchToVfs(QSharedPointer<OCC::Vfs> vfs)
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

    OCC::AccountPtr account() const { return _account; }
    OCC::SyncEngine &syncEngine() const { return *_syncEngine; }
    OCC::SyncJournalDb &syncJournal() const { return *_journalDb; }

    FileModifier &localModifier() { return _localModifier; }
    FileInfo &remoteModifier() { return _fakeQnam->currentRemoteState(); }
    FileInfo currentLocalState() {
        QDir rootDir{_tempDir.path()};
        FileInfo rootTemplate;
        fromDisk(rootDir, rootTemplate);
        rootTemplate.fixupParentPathRecursively();
        return rootTemplate;
    }

    FileInfo currentRemoteState() { return _fakeQnam->currentRemoteState(); }
    FileInfo &uploadState() { return _fakeQnam->uploadState(); }
    FileInfo dbState() const;

    struct ErrorList {
        FakeQNAM *_qnam;
        void append(const QString &path, int error = 500)
        { _qnam->errorPaths().insert(path, error); }
        void clear() { _qnam->errorPaths().clear(); }
    };
    ErrorList serverErrorPaths() { return {_fakeQnam}; }
    void setServerOverride(const FakeQNAM::Override &override) { _fakeQnam->setOverride(override); }

    QString localPath() const {
        // SyncEngine wants a trailing slash
        if (_tempDir.path().endsWith(QLatin1Char('/')))
            return _tempDir.path();
        return _tempDir.path() + QLatin1Char('/');
    }

    void scheduleSync() {
        // Have to be done async, else, an error before exec() does not terminate the event loop.
        QMetaObject::invokeMethod(_syncEngine.get(), "startSync", Qt::QueuedConnection);
    }

    void execUntilBeforePropagation() {
        QSignalSpy spy(_syncEngine.get(), SIGNAL(aboutToPropagate(SyncFileItemVector&)));
        QVERIFY(spy.wait());
    }

    void execUntilItemCompleted(const QString &relativePath) {
        QSignalSpy spy(_syncEngine.get(), SIGNAL(itemCompleted(const SyncFileItemPtr &)));
        QElapsedTimer t;
        t.start();
        while (t.elapsed() < 5000) {
            spy.clear();
            QVERIFY(spy.wait());
            for(const QList<QVariant> &args : spy) {
                auto item = args[0].value<OCC::SyncFileItemPtr>();
                if (item->destination() == relativePath)
                    return;
            }
        }
        QVERIFY(false);
    }

    bool execUntilFinished() {
        QSignalSpy spy(_syncEngine.get(), SIGNAL(finished(bool)));
        bool ok = spy.wait(3600000);
        Q_ASSERT(ok && "Sync timed out");
        return spy[0][0].toBool();
    }

    bool syncOnce() {
        scheduleSync();
        return execUntilFinished();
    }

private:
    static void toDisk(QDir &dir, const FileInfo &templateFi) {
        foreach (const FileInfo &child, templateFi.children) {
            if (child.isDir) {
                QDir subDir(dir);
                dir.mkdir(child.name);
                subDir.cd(child.name);
                toDisk(subDir, child);
            } else {
                QFile file{dir.filePath(child.name)};
                file.open(QFile::WriteOnly);
                file.write(QByteArray{}.fill(child.contentChar, child.size));
                file.close();
                OCC::FileSystem::setModTime(file.fileName(), OCC::Utility::qDateTimeToTime_t(child.lastModified));
            }
        }
    }

    static void fromDisk(QDir &dir, FileInfo &templateFi) {
        foreach (const QFileInfo &diskChild, dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot)) {
            if (diskChild.isDir()) {
                QDir subDir = dir;
                subDir.cd(diskChild.fileName());
                FileInfo &subFi = templateFi.children[diskChild.fileName()] = FileInfo{diskChild.fileName()};
                fromDisk(subDir, subFi);
            } else {
                QFile f{diskChild.filePath()};
                f.open(QFile::ReadOnly);
                auto content = f.read(1);
                if (content.size() == 0) {
                    qWarning() << "Empty file at:" << diskChild.filePath();
                    continue;
                }
                char contentChar = content.at(0);
                templateFi.children.insert(diskChild.fileName(), FileInfo{diskChild.fileName(), diskChild.size(), contentChar});
            }
        }
    }
};

static FileInfo &findOrCreateDirs(FileInfo &base, PathComponents components)
{
    if (components.isEmpty())
        return base;
    auto childName = components.pathRoot();
    auto it = base.children.find(childName);
    if (it != base.children.end()) {
        return findOrCreateDirs(*it, components.subComponents());
    }
    auto &newDir = base.children[childName] = FileInfo{childName};
    newDir.parentPath = base.path();
    return findOrCreateDirs(newDir, components.subComponents());
}

inline FileInfo FakeFolder::dbState() const
{
    FileInfo result;
    _journalDb->getFilesBelowPath("", [&](const OCC::SyncJournalFileRecord &record) {
        auto components = PathComponents(QString::fromUtf8(record._path));
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

/* Return the FileInfo for a conflict file for the specified relative filename */
inline const FileInfo *findConflict(FileInfo &dir, const QString &filename)
{
    QFileInfo info(filename);
    const FileInfo *parentDir = dir.find(info.path());
    if (!parentDir)
        return nullptr;
    QString start = info.baseName() + QStringLiteral(" (conflicted copy");
    for (const auto &item : parentDir->children) {
        if (item.name.startsWith(start)) {
            return &item;
        }
    }
    return nullptr;
}

struct ItemCompletedSpy : QSignalSpy {
    explicit ItemCompletedSpy(FakeFolder &folder)
        : QSignalSpy(&folder.syncEngine(), &OCC::SyncEngine::itemCompleted)
    {}

    OCC::SyncFileItemPtr findItem(const QString &path) const
    {
        for (const QList<QVariant> &args : *this) {
            auto item = args[0].value<OCC::SyncFileItemPtr>();
            if (item->destination() == path)
                return item;
        }
        return OCC::SyncFileItemPtr::create();
    }
};

// QTest::toString overloads
namespace OCC {
    inline char *toString(const SyncFileStatus &s) {
        return QTest::toString(QStringLiteral("SyncFileStatus(%1)").arg(s.toSocketAPIString()));
    }
}

inline void addFiles(QStringList &dest, const FileInfo &fi)
{
    if (fi.isDir) {
        dest += QStringLiteral("%1 - dir").arg(fi.path());
        foreach (const FileInfo &fi, fi.children)
            addFiles(dest, fi);
    } else {
        dest += QStringLiteral("%1 - %2 %3-bytes").arg(fi.path()).arg(fi.size).arg(fi.contentChar);
    }
}

inline QString toStringNoElide(const FileInfo &fi)
{
    QStringList files;
    foreach (const FileInfo &fi, fi.children)
        addFiles(files, fi);
    files.sort();
    return QStringLiteral("FileInfo with %1 files(\n\t%2\n)").arg(files.size()).arg(files.join(QStringLiteral("\n\t")));
}

inline char *toString(const FileInfo &fi)
{
    return QTest::toString(toStringNoElide(fi));
}

inline void addFilesDbData(QStringList &dest, const FileInfo &fi)
{
    // could include etag, permissions etc, but would need extra work
    if (fi.isDir) {
        dest += QStringLiteral("%1 - %2 %3 %4").arg(
            fi.name,
            fi.isDir ? QStringLiteral("dir") : QStringLiteral("file"),
            QString::number(fi.lastModified.toSecsSinceEpoch()),
            QString::fromUtf8(fi.fileId));
        foreach (const FileInfo &fi, fi.children)
            addFilesDbData(dest, fi);
    } else {
        dest += QStringLiteral("%1 - %2 %3 %4 %5").arg(
            fi.name,
            fi.isDir ? QStringLiteral("dir") : QStringLiteral("file"),
            QString::number(fi.size),
            QString::number(fi.lastModified.toSecsSinceEpoch()),
            QString::fromUtf8(fi.fileId));
    }
}

inline char *printDbData(const FileInfo &fi)
{
    QStringList files;
    foreach (const FileInfo &fi, fi.children)
        addFilesDbData(files, fi);
    return QTest::toString(QStringLiteral("FileInfo with %1 files(%2)").arg(files.size()).arg(files.join(QStringLiteral(", "))));
}
