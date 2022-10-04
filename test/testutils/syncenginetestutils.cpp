/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#include "syncenginetestutils.h"
#include "testutils.h"

#include "httplogger.h"
#include "accessmanager.h"
#include "libsync/configfile.h"

#include <thread>

using namespace std::chrono_literals;

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
    _processArguments.append({ "remove", relativePath });
}

void DiskFileModifier::insert(const QString &relativePath, quint64 size, char contentChar)
{
    // In the test helper, the insert command is implemented by filling a QByteArray, and then
    // writing that to disk. The size of the QByteArray is an int, so make sure that we don't end
    // up with a negative int when converting from quint64 -> int.
    Q_ASSERT(size <= std::numeric_limits<int>::max());

    _processArguments.append({ "insert", relativePath, QString::number(size), QChar::fromLatin1(contentChar) });
}

void DiskFileModifier::setContents(const QString &relativePath, quint64 newSize, char contentChar)
{
    _processArguments.append({ "contents", relativePath, QString::number(newSize), QChar::fromLatin1(contentChar) });
}

void DiskFileModifier::appendByte(const QString &relativePath, char contentChar)
{
    _processArguments.append({ "appendbyte", relativePath, QChar::fromLatin1(contentChar) });
}

void DiskFileModifier::mkdir(const QString &relativePath)
{
    _processArguments.append({ "mkdir", relativePath });
}

void DiskFileModifier::rename(const QString &from, const QString &to)
{
    _processArguments.append({ "rename", from, to });
}

void DiskFileModifier::setModTime(const QString &relativePath, const QDateTime &modTime)
{
    _processArguments.append({ "mtime", relativePath, QString::number(modTime.toSecsSinceEpoch()) });
}

class HelperProcess : public QProcess
{
public:
    HelperProcess(const QDir &rootDir, QStringList processArguments) // copy is on purpose
    {
        if (processArguments.isEmpty()) { // fast-path:
            finished = true;
            succeeded = true;
            return;
        }

        setProcessChannelMode(QProcess::MergedChannels);
        processArguments.prepend(rootDir.absolutePath());
        QObject::connect(this, &QProcess::readyRead, [this]() {
            while (canReadLine()) {
                qDebug() << "helper output:" << readLine();
            }
        });
        QObject::connect(this, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            [this](int exitCode, QProcess::ExitStatus exitStatus) {
                qDebug() << "helper finished:" << exitCode << exitStatus;
                this->finished = true;
                this->succeeded = exitStatus == QProcess::NormalExit && exitCode == 0;
            });

        qDebug() << "Starting helper:" << TEST_HELPER_EXE << processArguments;
        start(TEST_HELPER_EXE, processArguments);
    }

public:
    bool finished = false;
    bool succeeded = true;
};

bool DiskFileModifier::applyModifications()
{
    if (_processArguments.isEmpty()) {
        return true;
    }

    HelperProcess helper(_rootDir, _processArguments);
    helper.waitForStarted();
    _processArguments.clear(); // the helper has a copy, we clean it now for the next run
    do {
        std::this_thread::sleep_for(50ms);
        QThread::currentThread()->eventDispatcher()->processEvents(QEventLoop::AllEvents);
    } while (!helper.finished);
    return helper.succeeded;
}

bool DiskFileModifier::applyModificationsAndSync(FakeFolder &ff, OCC::Vfs::Mode mode)
{
    if (mode == OCC::Vfs::Off || mode == OCC::Vfs::WithSuffix) { // Classic mode:
        if (!applyModifications()) {
            return false;
        }
        return ff.syncOnce();
    }

    // Non-classic mode:

    HelperProcess p(_rootDir, _processArguments);
    _processArguments.clear();

    do {
        // process any pending events
        QThread::currentThread()->eventDispatcher()->processEvents(QEventLoop::AllEvents);
        // wait a bit to get the helper started with it's work
        std::this_thread::sleep_for(50ms);
        // process any output from the helper
        QThread::currentThread()->eventDispatcher()->processEvents(QEventLoop::AllEvents);
        // now we might need to sync
        if (!ff.syncOnce()) {
            return false;
        }
        QThread::currentThread()->eventDispatcher()->processEvents(QEventLoop::AllEvents);
    } while (!p.finished);

    return p.succeeded;
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

FileInfo &FileInfo::addChild(const FileInfo &info)
{
    FileInfo &dest = this->children[info.name] = info;
    dest.parentPath = path();
    dest.fixupParentPathRecursively();
    return dest;
}

void FileInfo::remove(const QString &relativePath)
{
    const PathComponents pathComponents { relativePath };
    FileInfo *parent = findInvalidatingEtags(pathComponents.parentDirComponents());
    Q_ASSERT(parent);
    parent->children.erase(std::find_if(parent->children.begin(), parent->children.end(),
        [&pathComponents](const FileInfo &fi) { return fi.name == pathComponents.fileName(); }));
}

void FileInfo::insert(const QString &relativePath, quint64 size, char contentChar)
{
    create(relativePath, size, contentChar);
}

void FileInfo::setContents(const QString &relativePath, quint64 newSize, char contentChar)
{
    FileInfo *file = findInvalidatingEtags(relativePath);
    Q_ASSERT(file);
    file->contentChar = contentChar;
    file->contentSize = newSize;
    if (!file->isDehydratedPlaceholder) {
        file->fileSize = newSize;
    }
}

void FileInfo::appendByte(const QString &relativePath, char contentChar)
{
    Q_UNUSED(contentChar)
    FileInfo *file = findInvalidatingEtags(relativePath);
    Q_ASSERT(file);
    if (!file->isDehydratedPlaceholder) {
        file->contentSize += 1;
    }
    file->fileSize += 1;
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
    file->setLastModified(modTime);
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

FileInfo *FileInfo::create(const QString &relativePath, quint64 size, char contentChar)
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

bool FileInfo::equals(const FileInfo &other, CompareWhat compareWhat) const
{
    // Only check the content and contentSize if both files are hydrated:
    if (!isDehydratedPlaceholder && !other.isDehydratedPlaceholder) {
        if (contentSize != other.contentSize || contentChar != other.contentChar) {
            qDebug() << "1" << name << "!=" << other.name;
            return false;
        }
    }

    // We need to check this before we use isDir in the next if-statement:
    if (isDir != other.isDir) {
        qDebug() << "2" << name << "!=" << other.name;
        return false;
    }

    if (compareWhat == CompareLastModified) {
        // Don't check directory mtime: it might change when (unsynced) files get created.
        if (!isDir && _lastModifiedInSecondsUTC != other._lastModifiedInSecondsUTC) {
            qDebug() << "3" << name << "!=" << other.name;
            return false;
        }
    }

    if (name != other.name || fileSize != other.fileSize) {
        qDebug() << "4" << name << "!=" << other.name;
        return false;
    }

    if (children.size() != other.children.size()) {
        qDebug() << "5" << name << "!=" << other.name;
        return false;
    }

    for (auto it = children.constBegin(), eit = children.constEnd(); it != eit; ++it) {
        auto oit = other.children.constFind(it.key());
        if (oit == other.children.constEnd()) {
            qDebug() << "6" << name << "!=" << other.name;
            return false;
        } else if (!it.value().equals(oit.value(), compareWhat)) {
            qDebug() << "7" << name << "!=" << other.name;
            return false;
        }
    }

    return true;
}

QString FileInfo::path() const
{
    return (parentPath.isEmpty() ? QString() : (parentPath + QLatin1Char('/'))) + name;
}

QString FileInfo::absolutePath() const
{
    if (parentPath.endsWith(QLatin1Char('/'))) {
        return parentPath + name;
    } else {
        return parentPath + QLatin1Char('/') + name;
    }
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
    QBuffer buffer { &payload };
    buffer.open(QIODevice::WriteOnly);
    QXmlStreamWriter xml(&buffer);
    xml.writeNamespace(davUri, QStringLiteral("d"));
    xml.writeNamespace(ocUri, QStringLiteral("oc"));
    xml.writeStartDocument();
    xml.writeStartElement(davUri, QStringLiteral("multistatus"));
    auto writeFileResponse = [&](const FileInfo &fileInfo) {
        xml.writeStartElement(davUri, QStringLiteral("response"));
        const auto href = OCC::Utility::concatUrlPath(prefix, QString::fromUtf8(QUrl::toPercentEncoding(fileInfo.absolutePath(), "/"))).path();
        xml.writeTextElement(davUri, QStringLiteral("href"), href);
        xml.writeStartElement(davUri, QStringLiteral("propstat"));
        xml.writeStartElement(davUri, QStringLiteral("prop"));

        if (fileInfo.isDir) {
            xml.writeStartElement(davUri, QStringLiteral("resourcetype"));
            xml.writeEmptyElement(davUri, QStringLiteral("collection"));
            xml.writeEndElement(); // resourcetype
        } else
            xml.writeEmptyElement(davUri, QStringLiteral("resourcetype"));

        auto gmtDate = fileInfo.lastModifiedInUtc();
        auto stringDate = QLocale::c().toString(gmtDate, QStringLiteral("ddd, dd MMM yyyy HH:mm:ss 'GMT'"));
        xml.writeTextElement(davUri, QStringLiteral("getlastmodified"), stringDate);
        xml.writeTextElement(davUri, QStringLiteral("getcontentlength"), QString::number(fileInfo.contentSize));
        xml.writeTextElement(davUri, QStringLiteral("getetag"), QStringLiteral("\"%1\"").arg(QString::fromLatin1(fileInfo.etag)));
        xml.writeTextElement(ocUri, QStringLiteral("permissions"), !fileInfo.permissions.isNull() ? QString(fileInfo.permissions.toString()) : fileInfo.isShared ? QStringLiteral("SRDNVCKW")
                                                                                                                                                                 : QStringLiteral("RDNVCKW"));
        xml.writeTextElement(ocUri, QStringLiteral("id"), QString::fromUtf8(fileInfo.fileId));
        xml.writeTextElement(ocUri, QStringLiteral("checksums"), QString::fromUtf8(fileInfo.checksums));
        buffer.write(fileInfo.extraDavProperties);
        xml.writeEndElement(); // prop
        xml.writeTextElement(davUri, QStringLiteral("status"), QStringLiteral("HTTP/1.1 200 OK"));
        xml.writeEndElement(); // propstat
        xml.writeEndElement(); // response
    };

    writeFileResponse(*fileInfo);

    const int depth = request.rawHeader(QByteArrayLiteral("Depth")).toInt();
    if (depth > 0) {
        for (const FileInfo &childFileInfo : fileInfo->children) {
            writeFileResponse(childFileInfo);
        }
    }
    xml.writeEndElement(); // multistatus
    xml.writeEndDocument();

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
        fileInfo->contentSize = putPayload.size();
        fileInfo->contentChar = putPayload.at(0);
    } else {
        // Assume that the file is filled with the same character
        fileInfo = remoteRootFileInfo.create(fileName, putPayload.size(), putPayload.at(0));
    }
    fileInfo->fileSize = fileInfo->contentSize; // it's hydrated on the server, so these are the same
    fileInfo->setLastModifiedFromSecondsUTC(request.rawHeader("X-OC-Mtime").toLongLong());
    remoteRootFileInfo.find(fileName, /*invalidate_etags=*/true);
    return fileInfo;
}

void FakePutReply::respond()
{
    emit uploadProgress(fileInfo->contentSize, fileInfo->contentSize);
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
        qDebug() << "Could not find file" << fileName << "on the remote";
        state = State::FileNotFound;
    }
    QMetaObject::invokeMethod(this, &FakeGetReply::respond, Qt::QueuedConnection);
}

void FakeGetReply::respond()
{
    switch (state) {
    case State::Aborted:
        setError(OperationCanceledError, QStringLiteral("Operation Canceled"));
        emit metaDataChanged();
        break;
    case State::FileNotFound:
        setError(ContentNotFoundError, QStringLiteral("File Not Found"));
        emit metaDataChanged();
        break;
    case State::Ok:
        payload = fileInfo->contentChar;
        size = fileInfo->contentSize;
        setHeader(QNetworkRequest::ContentLengthHeader, size);
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);
        setRawHeader("OC-ETag", fileInfo->etag);
        setRawHeader("ETag", fileInfo->etag);
        setRawHeader("OC-FileId", fileInfo->fileId);
        setRawHeader("X-OC-Mtime", QByteArray::number(fileInfo->lastModifiedInSecondsUTC()));
        emit metaDataChanged();
        if (bytesAvailable()) {
            emit readyRead();
        }
    }
    emit finished();
}

void FakeGetReply::abort()
{
    setError(OperationCanceledError, QStringLiteral("Operation Canceled"));
    state = State::Aborted;
}

qint64 FakeGetReply::bytesAvailable() const
{
    switch (state) {
    case State::Ok:
        return size + QIODevice::bytesAvailable();
    case State::Aborted:
        Q_FALLTHROUGH();
    case State::FileNotFound:
        return 0;
    }
    Q_UNREACHABLE(); // Unreachable, but GCC on CentOS 7 does not understand that.
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
    setRawHeader("X-OC-Mtime", QByteArray::number(fileInfo->lastModifiedInSecondsUTC()));
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

    const auto &sourceFolderChildren = sourceFolder->children;
    // Compute the size and content from the chunks if possible
    for (auto it = sourceFolderChildren.cbegin(); it != sourceFolderChildren.cend(); ++it) {
        const auto &chunkNameLongLong = it.key().toLongLong();
        const auto &x = it.value();
        if (chunkNameLongLong != prev)
            break;
        Q_ASSERT(!x.isDir);
        Q_ASSERT(x.contentSize > 0); // There should not be empty chunks
        size += x.contentSize;
        Q_ASSERT(!payload || payload == x.contentChar);
        payload = x.contentChar;
        ++count;
        prev = chunkNameLongLong + x.contentSize;
    }
    Q_ASSERT(sourceFolderChildren.count() == count); // There should not be holes or extra files

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
        fileInfo->contentSize = size;
        fileInfo->contentChar = payload;
        fileInfo->fileSize = fileInfo->contentSize; // it's hydrated on the server, so these are the same
    } else {
        Q_ASSERT(!request.hasRawHeader("If"));
        // Assume that the file is filled with the same character
        fileInfo = remoteRootFileInfo.create(fileName, size, payload);
    }
    fileInfo->setLastModifiedFromSecondsUTC(request.rawHeader("X-OC-Mtime").toLongLong());
    remoteRootFileInfo.find(fileName, /*invalidate_etags=*/true);

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
    : FakeReply { parent }
    , _body(body)
{
    setRequest(request);
    setUrl(request.url());
    setOperation(op);
    open(QIODevice::ReadOnly);
    QTimer::singleShot(10ms, this, &FakePayloadReply::respond);
}

void FakePayloadReply::respond()
{
    setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);
    setHeader(QNetworkRequest::ContentLengthHeader, _body.size());
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
    QTimer::singleShot(5ms, this, &FakeErrorReply::slotSetFinished);
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
    emit error(OperationCanceledError);
    setFinished(true);
    emit finished();
}

FakeAM::FakeAM(FileInfo initialRoot)
    : _remoteRootFileInfo { std::move(initialRoot) }
{
    setCookieJar(new OCC::CookieJar);
}

QNetworkReply *FakeAM::createRequest(QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *outgoingData)
{
    QNetworkReply *reply = nullptr;
    auto newRequest = request;
    newRequest.setRawHeader("X-Request-ID", OCC::AccessManager::generateRequestId());
    if (_override) {
        if (auto _reply = _override(op, newRequest, outgoingData)) {
            reply = _reply;
        }
    }
    if (!reply) {
        const QString fileName = getFilePathFromUrl(newRequest.url());
        Q_ASSERT(!fileName.isNull());
        if (_errorPaths.contains(fileName)) {
            reply = new FakeErrorReply { op, newRequest, this, _errorPaths[fileName] };
        }
    }
    if (!reply) {
        const bool isUpload = newRequest.url().path().startsWith(sUploadUrl.path());
        FileInfo &info = isUpload ? _uploadFileInfo : _remoteRootFileInfo;

        auto verb = newRequest.attribute(QNetworkRequest::CustomVerbAttribute);
        if (verb == QLatin1String("PROPFIND"))
            // Ignore outgoingData always returning somethign good enough, works for now.
            reply = new FakePropfindReply { info, op, newRequest, this };
        else if (verb == QLatin1String("GET") || op == QNetworkAccessManager::GetOperation)
            reply = new FakeGetReply { info, op, newRequest, this };
        else if (verb == QLatin1String("PUT") || op == QNetworkAccessManager::PutOperation)
            reply = new FakePutReply { info, op, newRequest, outgoingData->readAll(), this };
        else if (verb == QLatin1String("MKCOL"))
            reply = new FakeMkcolReply { info, op, newRequest, this };
        else if (verb == QLatin1String("DELETE") || op == QNetworkAccessManager::DeleteOperation)
            reply = new FakeDeleteReply { info, op, newRequest, this };
        else if (verb == QLatin1String("MOVE") && !isUpload)
            reply = new FakeMoveReply { info, op, newRequest, this };
        else if (verb == QLatin1String("MOVE") && isUpload)
            reply = new FakeChunkMoveReply { info, _remoteRootFileInfo, op, newRequest, this };
        else {
            qDebug() << verb << outgoingData;
            Q_UNREACHABLE();
        }
    }
    // timeout would be handled by Qt
    if (request.transferTimeout() != 0) {
        QTimer::singleShot(request.transferTimeout(), reply, [reply] {
            reply->abort();
        });
    }
    OCC::HttpLogger::logRequest(reply, op, outgoingData);
    return reply;
}

FakeFolder::FakeFolder(const FileInfo &fileTemplate, OCC::Vfs::Mode vfsMode, bool filesAreDehydrated)
    : _localModifier(_tempDir.path())
{
    // Needs to be done once
    OCC::SyncEngine::minimumFileAgeForUpload = std::chrono::milliseconds(0);

    QDir rootDir { _tempDir.path() };
    qDebug() << "FakeFolder operating on" << rootDir;
    toDisk(rootDir, filesAreDehydrated ? FileInfo() : fileTemplate);

    _fakeAm = new FakeAM(fileTemplate);
    _account = OCC::TestUtils::createDummyAccount();
    _account->setCredentials(new FakeCredentials { _fakeAm });

    _journalDb.reset(new OCC::SyncJournalDb(localPath() + QStringLiteral(".sync_test.db")));
    // TODO: davUrl

    _syncEngine.reset(new OCC::SyncEngine(_account, _account->davUrl(), localPath(), QString(), _journalDb.get()));
    _syncEngine->setSyncOptions(OCC::SyncOptions { QSharedPointer<OCC::Vfs>(OCC::createVfsFromPlugin(vfsMode).release()) });

    // Ignore temporary files from the download. (This is in the default exclude list, but we don't load it)
    _syncEngine->excludedFiles().addManualExclude(QStringLiteral("]*.~*"));

    // handle aboutToRemoveAllFiles with a timeout in case our test does not handle it
    QObject::connect(_syncEngine.get(), &OCC::SyncEngine::aboutToRemoveAllFiles, _syncEngine.get(), [this](OCC::SyncFileItem::Direction, const std::function<void(bool)> &callback) {
        QTimer::singleShot(1s, _syncEngine.get(), [callback] {
            callback(false);
        });
    });

    auto vfs = _syncEngine->syncOptions()._vfs;
    if (vfsMode != vfs->mode()) {
        vfs.reset(createVfsFromPlugin(vfsMode).release());
        Q_ASSERT(vfs);
    }

    // Ensure we have a valid Vfs instance "running"
    switchToVfs(vfs);

    if (vfsMode != OCC::Vfs::Off) {
        const auto pinState = filesAreDehydrated ? OCC::PinState::OnlineOnly : OCC::PinState::AlwaysLocal;
        syncJournal().internalPinStates().setForPath("", pinState);
        OC_ENFORCE(vfs->setPinState("", pinState));
    }

    // A new folder will update the local file state database on first sync.
    // To have a state matching what users will encounter, we have to a sync
    // using an identical local/remote file tree first.
    OC_ENFORCE(syncOnce())
}

void FakeFolder::switchToVfs(QSharedPointer<OCC::Vfs> vfs)
{
    auto opts = _syncEngine->syncOptions();

    opts._vfs->stop();
    QObject::disconnect(_syncEngine.get(), nullptr, opts._vfs.data(), nullptr);

    opts._vfs = vfs;
    _syncEngine->setSyncOptions(opts);

    OCC::VfsSetupParams vfsParams(_account, _account->davUrl(), false);
    vfsParams.filesystemPath = localPath();
    vfsParams.remotePath = QLatin1Char('/');
    vfsParams.journal = _journalDb.get();
    vfsParams.providerName = QStringLiteral("OC-TEST");
    vfsParams.providerDisplayName = QStringLiteral("OC-TEST");
    vfsParams.providerVersion = QVersionNumber(0, 1, 0);
    vfsParams.multipleAccountsRegistered = false;
    QObject::connect(_syncEngine.get(), &QObject::destroyed, vfs.data(), [vfs]() {
        vfs->stop();
        vfs->unregisterFolder();
    });
    QObject::connect(&_syncEngine->syncFileStatusTracker(), &OCC::SyncFileStatusTracker::fileStatusChanged,
        vfs.data(), &OCC::Vfs::fileStatusChanged);

    QObject::connect(vfs.get(), &OCC::Vfs::error, vfs.get(), [](const QString &error) {
        QFAIL(qUtf8Printable(error));
    });
    QSignalSpy spy(vfs.get(), &OCC::Vfs::started);
    vfs->start(vfsParams);

    // don't use QVERIFY outside of the test slot
    if (spy.isEmpty() && !spy.wait()) {
        QFAIL("VFS Setup failed");
    }
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
    if (_tempDir.path().endsWith(QLatin1Char('/')))
        return _tempDir.path();
    return _tempDir.path() + QLatin1Char('/');
}

void FakeFolder::scheduleSync()
{
    // Have to be done async, else, an error before exec() does not terminate the event loop.
    QMetaObject::invokeMethod(_syncEngine.get(), &OCC::SyncEngine::startSync, Qt::QueuedConnection);
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

bool FakeFolder::isDehydratedPlaceholder(const QString &filePath)
{
    return vfs()->isDehydratedPlaceholder(filePath);
}

QSharedPointer<OCC::Vfs> FakeFolder::vfs() const
{
    return _syncEngine->syncOptions()._vfs;
}

void FakeFolder::toDisk(QDir &dir, const FileInfo &templateFi)
{
    for (const auto &child : templateFi.children) {
        if (child.isDir) {
            QDir subDir(dir);
            dir.mkdir(child.name);
            subDir.cd(child.name);
            toDisk(subDir, child);
        } else {
            QFile file { dir.filePath(child.name) };
            file.open(QFile::WriteOnly);
            file.write(QByteArray {}.fill(child.contentChar, child.contentSize));
            file.close();
            OCC::FileSystem::setModTime(file.fileName(), child.lastModifiedInSecondsUTC());
        }
    }
}

void FakeFolder::fromDisk(QDir &dir, FileInfo &templateFi)
{
    const auto infoList = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot);
    for (const auto &diskChild : infoList) {
        if (diskChild.isDir()) {
            QDir subDir = dir;
            subDir.cd(diskChild.fileName());
            FileInfo &subFi = templateFi.children[diskChild.fileName()] = FileInfo { diskChild.fileName() };
            subFi.setLastModified(diskChild.lastModified());
            fromDisk(subDir, subFi);
        } else {
            FileInfo fi(diskChild.fileName());
            fi.isDir = false;
            fi.fileSize = diskChild.size();
            fi.isDehydratedPlaceholder = isDehydratedPlaceholder(diskChild.absoluteFilePath());
            fi.setLastModified(diskChild.lastModified());
            if (fi.isDehydratedPlaceholder) {
                fi.contentChar = '\0';
                fi.contentSize = 0;
            } else {
                QFile f { diskChild.filePath() };
                OC_ENFORCE(f.open(QFile::ReadOnly));
                auto content = f.read(1);
                if (content.size() == 0) {
                    qWarning() << "Empty file at:" << diskChild.filePath();
                    continue;
                }
                fi.contentChar = content.at(0);
                fi.contentSize = fi.fileSize;
            }

            templateFi.children.insert(fi.name, fi);
        }
    }
}

FileInfo &findOrCreateDirs(FileInfo &base, const PathComponents &components)
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
    _journalDb->getFilesBelowPath("", [&](const OCC::SyncJournalFileRecord &record) {
        auto components = PathComponents(QString::fromUtf8(record._path));
        auto &parentDir = findOrCreateDirs(result, components.parentDirComponents());
        auto name = components.fileName();
        auto &item = parentDir.children[name];
        item.name = name;
        item.parentPath = parentDir.path();
        item.contentSize = record._fileSize;
        item.isDir = record._type == ItemTypeDirectory;
        item.permissions = record._remotePerm;
        item.etag = record._etag;
        item.setLastModifiedFromSecondsUTC(record._modtime);
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
    return nullptr;
}

FakeReply::FakeReply(QObject *parent)
    : QNetworkReply(parent)
{
    setRawHeader(QByteArrayLiteral("Date"), QDateTime::currentDateTimeUtc().toString(Qt::RFC2822Date).toUtf8());
}

FakeReply::~FakeReply()
{
}
