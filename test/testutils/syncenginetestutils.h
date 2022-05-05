/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */
#pragma once

#include "accessmanager.h"
#include "account.h"
#include "common/syncjournaldb.h"
#include "common/syncjournalfilerecord.h"
#include "common/vfs.h"
#include "creds/abstractcredentials.h"
#include "csync_exclude.h"
#include "filesystem.h"
#include "folder.h"
#include "logger.h"
#include "syncengine.h"
#include <cstring>

#include <QDir>
#include <QNetworkReply>
#include <QMap>
#include <QtTest>
#include <cookiejar.h>
#include <QTimer>

#include <chrono>
/*
 * TODO: In theory we should use QVERIFY instead of Q_ASSERT for testing, but this
 * only works when directly called from a QTest :-(
 */


static const QUrl sRootUrl = QUrl::fromEncoded("owncloud://somehost/owncloud/remote.php/webdav/");
static const QUrl sRootUrl2 = QUrl::fromEncoded("owncloud://somehost/owncloud/remote.php/dav/files/admin/");
static const QUrl sUploadUrl = QUrl::fromEncoded("owncloud://somehost/owncloud/remote.php/dav/uploads/admin/");

inline QString getFilePathFromUrl(const QUrl &url)
{
    QString path = url.path();
    if (path.startsWith(sRootUrl.path()))
        return path.mid(sRootUrl.path().length());
    if (path.startsWith(sRootUrl2.path()))
        return path.mid(sRootUrl2.path().length());
    if (path.startsWith(sUploadUrl.path()))
        return path.mid(sUploadUrl.path().length());
    return {};
}


inline QByteArray generateEtag()
{
    return QByteArray::number(QDateTime::currentDateTimeUtc().toMSecsSinceEpoch(), 16) + QByteArray::number(QRandomGenerator::global()->generate(), 16);
}
inline QByteArray generateFileId()
{
    return QByteArray::number(QRandomGenerator::global()->generate(), 16);
}

class PathComponents : public QStringList
{
public:
    PathComponents(const char *path);
    PathComponents(const QString &path);
    PathComponents(const QStringList &pathComponents);

    PathComponents parentDirComponents() const;
    PathComponents subComponents() const &;
    PathComponents subComponents() &&
    {
        removeFirst();
        return std::move(*this);
    }
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
    DiskFileModifier(const QString &rootDirPath)
        : _rootDir(rootDirPath)
    {
    }
    void remove(const QString &relativePath) override;
    void insert(const QString &relativePath, qint64 size = 64, char contentChar = 'W') override;
    void setContents(const QString &relativePath, char contentChar) override;
    void appendByte(const QString &relativePath, char contentChar) override;
    void modifyByte(const QString &relativePath, quint64 offset, char contentChar) override;

    void mkdir(const QString &relativePath) override;
    void rename(const QString &from, const QString &to) override;
    void setModTime(const QString &relativePath, const QDateTime &modTime) override;
};

static inline qint64 defaultLastModified()
{
    auto precise = QDateTime::currentDateTimeUtc().addDays(-7);
    time_t timeInSeconds = OCC::Utility::qDateTimeToTime_t(precise);
    return timeInSeconds;
}

/// FIXME: we should make it explicit in the construtor if we're talking about a hydrated or a dehydrated file!
class FileInfo : public FileModifier
{
public:
    static FileInfo A12_B12_C12_S12();

    FileInfo() = default;
    FileInfo(const QString &name)
        : name { name }
    {
    }
    FileInfo(const QString &name, qint64 size)
        : name { name }
        , isDir { false }
        , fileSize(size)
        , contentSize { size }
    {
    }
    FileInfo(const QString &name, qint64 size, char contentChar)
        : name { name }
        , isDir { false }
        , fileSize(size)
        , contentSize { size }
        , contentChar { contentChar }
    {
    }
    FileInfo(const QString &name, const std::initializer_list<FileInfo> &children);

    FileInfo &addChild(const FileInfo &info);

    void remove(const QString &relativePath) override;

    void insert(const QString &relativePath, qint64 size = 64, char contentChar = 'W') override;

    void setContents(const QString &relativePath, char contentChar) override;

    void appendByte(const QString &relativePath, char contentChar = 0) override;

    void modifyByte(const QString &relativePath, quint64 offset, char contentChar) override;

    void mkdir(const QString &relativePath) override;

    void rename(const QString &oldPath, const QString &newPath) override;

    void setModTime(const QString &relativePath, const QDateTime &modTime) override;

    /// Return a pointer to the FileInfo, or a nullptr if it doesn't exist
    FileInfo *find(PathComponents pathComponents, const bool invalidateEtags = false);

    FileInfo *createDir(const QString &relativePath);

    FileInfo *create(const QString &relativePath, qint64 size, char contentChar);

    bool operator<(const FileInfo &other) const
    {
        return name < other.name;
    }

    enum CompareWhat {
        CompareLastModified,
        IgnoreLastModified,
    };

    bool operator==(const FileInfo &other) const { return equals(other, CompareLastModified); }

    bool equals(const FileInfo &other, CompareWhat compareWhat) const;

    bool operator!=(const FileInfo &other) const
    {
        return !operator==(other);
    }

    QDateTime lastModified() const
    {
        return QDateTime::fromSecsSinceEpoch(_lastModifiedInSecondsUTC, Qt::LocalTime);
    }

    QDateTime lastModifiedInUtc() const
    {
        return QDateTime::fromSecsSinceEpoch(_lastModifiedInSecondsUTC, Qt::UTC);
    }

    void setLastModified(const QDateTime &t)
    {
        _lastModifiedInSecondsUTC = t.toSecsSinceEpoch();
    }

    qint64 lastModifiedInSecondsUTC() const
    {
        return _lastModifiedInSecondsUTC;
    }

    void setLastModifiedFromSecondsUTC(qint64 utcSecs)
    {
        _lastModifiedInSecondsUTC = utcSecs;
    }

    QString path() const;
    QString absolutePath() const;

    void fixupParentPathRecursively();

    QString name;
    bool isDir = true;
    bool isShared = false;
    OCC::RemotePermissions permissions; // When uset, defaults to everything
    qint64 _lastModifiedInSecondsUTC = defaultLastModified();
    QByteArray etag = generateEtag();
    QByteArray fileId = generateFileId();
    QByteArray checksums;
    QByteArray extraDavProperties;
    qint64 fileSize = 0;
    qint64 contentSize = 0;
    char contentChar = 'W';
    bool isDehydratedPlaceholder = false;

    // Sorted by name to be able to compare trees
    QMap<QString, FileInfo> children;
    QString parentPath;

    FileInfo *findInvalidatingEtags(PathComponents pathComponents);

    friend inline QDebug operator<<(QDebug dbg, const FileInfo &fi)
    {
        return dbg.nospace().noquote()
            << "{ '" << fi.path() << "': "
            << ", isDir:" << fi.isDir
            << QStringLiteral(", lastModified: %1 (%2)").arg(QString::number(fi._lastModifiedInSecondsUTC), fi.lastModifiedInUtc().toString())
            << ", fileSize:" << fi.fileSize
            << ", contentSize:" << fi.contentSize
            << QStringLiteral(", contentChar: 0x%1").arg(QString::number(int(fi.contentChar), 16))
            << ", isDehydratedPlaceholder:" << fi.isDehydratedPlaceholder
            << ", children:" << fi.children
            << " }";
    }
};

class FakeReply : public QNetworkReply
{
    Q_OBJECT
public:
    FakeReply(QObject *parent);
    virtual ~FakeReply();

    // useful to be public for testing
    using QNetworkReply::setRawHeader;
};

class FakePropfindReply : public FakeReply
{
    Q_OBJECT
public:
    QByteArray payload;

    FakePropfindReply(FileInfo &remoteRootFileInfo, QNetworkAccessManager::Operation op, const QNetworkRequest &request, QObject *parent);

    Q_INVOKABLE void respond();

    Q_INVOKABLE void respond404();

    void abort() override { }

    qint64 bytesAvailable() const override;
    qint64 readData(char *data, qint64 maxlen) override;
};

class FakePutReply : public FakeReply
{
    Q_OBJECT
    FileInfo *fileInfo;

public:
    FakePutReply(FileInfo &remoteRootFileInfo, QNetworkAccessManager::Operation op, const QNetworkRequest &request, const QByteArray &putPayload, QObject *parent);

    static FileInfo *perform(FileInfo &remoteRootFileInfo, const QNetworkRequest &request, const QByteArray &putPayload);

    Q_INVOKABLE virtual void respond();

    void abort() override;
    qint64 readData(char *, qint64) override { return 0; }
};

class FakeMkcolReply : public FakeReply
{
    Q_OBJECT
    FileInfo *fileInfo;

public:
    FakeMkcolReply(FileInfo &remoteRootFileInfo, QNetworkAccessManager::Operation op, const QNetworkRequest &request, QObject *parent);

    Q_INVOKABLE void respond();

    void abort() override { }
    qint64 readData(char *, qint64) override { return 0; }
};

class FakeDeleteReply : public FakeReply
{
    Q_OBJECT
public:
    FakeDeleteReply(FileInfo &remoteRootFileInfo, QNetworkAccessManager::Operation op, const QNetworkRequest &request, QObject *parent);

    Q_INVOKABLE void respond();

    void abort() override { }
    qint64 readData(char *, qint64) override { return 0; }
};

class FakeMoveReply : public FakeReply
{
    Q_OBJECT
public:
    FakeMoveReply(FileInfo &remoteRootFileInfo, QNetworkAccessManager::Operation op, const QNetworkRequest &request, QObject *parent);

    Q_INVOKABLE void respond();

    void abort() override { }
    qint64 readData(char *, qint64) override { return 0; }
};

class FakeGetReply : public FakeReply
{
    Q_OBJECT
public:
    const FileInfo *fileInfo;
    char payload;
    int size;
    bool aborted = false;

    FakeGetReply(FileInfo &remoteRootFileInfo, QNetworkAccessManager::Operation op, const QNetworkRequest &request, QObject *parent);

    Q_INVOKABLE void respond();

    void abort() override;
    qint64 bytesAvailable() const override;

    qint64 readData(char *data, qint64 maxlen) override;
};

class FakeGetWithDataReply : public FakeReply
{
    Q_OBJECT
public:
    const FileInfo *fileInfo;
    QByteArray payload;
    quint64 offset = 0;
    bool aborted = false;

    FakeGetWithDataReply(FileInfo &remoteRootFileInfo, const QByteArray &data, QNetworkAccessManager::Operation op, const QNetworkRequest &request, QObject *parent);

    Q_INVOKABLE void respond();

    void abort() override;
    qint64 bytesAvailable() const override;

    qint64 readData(char *data, qint64 maxlen) override;
};

class FakeChunkMoveReply : public FakeReply
{
    Q_OBJECT
    FileInfo *fileInfo;

public:
    FakeChunkMoveReply(FileInfo &uploadsFileInfo, FileInfo &remoteRootFileInfo,
        QNetworkAccessManager::Operation op, const QNetworkRequest &request,
        QObject *parent);

    static FileInfo *perform(FileInfo &uploadsFileInfo, FileInfo &remoteRootFileInfo, const QNetworkRequest &request);

    Q_INVOKABLE virtual void respond();

    Q_INVOKABLE void respondPreconditionFailed();

    void abort() override;

    qint64 readData(char *, qint64) override { return 0; }
};

class FakePayloadReply : public FakeReply
{
    Q_OBJECT
public:
    FakePayloadReply(QNetworkAccessManager::Operation op, const QNetworkRequest &request,
        const QByteArray &body, QObject *parent);

    void respond();

    void abort() override { }
    qint64 readData(char *buf, qint64 max) override;
    qint64 bytesAvailable() const override;
    QByteArray _body;
};


class FakeErrorReply : public FakeReply
{
    Q_OBJECT
public:
    FakeErrorReply(QNetworkAccessManager::Operation op, const QNetworkRequest &request,
        QObject *parent, int httpErrorCode, const QByteArray &body = QByteArray());

    Q_INVOKABLE virtual void respond();

    // make public to give tests easy interface
    using QNetworkReply::setAttribute;
    using QNetworkReply::setError;

public slots:
    void slotSetFinished();

public:
    void abort() override { }
    qint64 readData(char *buf, qint64 max) override;
    qint64 bytesAvailable() const override;

    QByteArray _body;
};

// A reply that never responds
class FakeHangingReply : public FakeReply
{
    Q_OBJECT
public:
    FakeHangingReply(QNetworkAccessManager::Operation op, const QNetworkRequest &request, QObject *parent);

    void abort() override;
    qint64 readData(char *, qint64) override { return 0; }
};

// A delayed reply
template <class OriginalReply>
class DelayedReply : public OriginalReply
{
public:
    template <typename... Args>
    explicit DelayedReply(const std::chrono::milliseconds delayMS, Args &&...args)
        : OriginalReply(std::forward<Args>(args)...)
        , _delayMs(delayMS)
    {
    }
    std::chrono::milliseconds _delayMs;

    void respond() override
    {
        QTimer::singleShot(_delayMs, static_cast<OriginalReply *>(this), [this] {
            // Explicit call to bases's respond();
            this->OriginalReply::respond();
        });
    }
};

class FakeAM : public OCC::AccessManager
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
    FakeAM(FileInfo initialRoot);
    FileInfo &currentRemoteState() { return _remoteRootFileInfo; }
    FileInfo &uploadState() { return _uploadFileInfo; }

    QHash<QString, int> &errorPaths() { return _errorPaths; }

    void setOverride(const Override &override) { _override = override; }

protected:
    QNetworkReply *createRequest(Operation op, const QNetworkRequest &request,
        QIODevice *outgoingData = nullptr) override;
};

class FakeCredentials : public OCC::AbstractCredentials
{
public:
    FakeCredentials(OCC::AccessManager *am)
        : _am { am }
    {
    }

    QString authType() const override { return QStringLiteral("test"); }
    QString user() const override { return QStringLiteral("admin"); }
    OCC::AccessManager *createAM() const override { return _am; }
    bool ready() const override { return true; }
    void fetchFromKeychain() override { }
    void askFromUser() override { }
    bool stillValid(QNetworkReply *) override { return true; }
    void persist() override { }
    void invalidateToken() override { }
    void forgetSensitiveData() override { }

private:
    OCC::AccessManager *_am;
};

class FakeFolder
{
    QTemporaryDir _tempDir;
    DiskFileModifier _localModifier;
    // FIXME: Clarify ownership, double delete
    FakeAM *_fakeAm;
    OCC::AccountPtr _account;
    std::unique_ptr<OCC::SyncJournalDb> _journalDb;
    std::unique_ptr<OCC::SyncEngine> _syncEngine;
    OCC::Vfs::Mode _vfsMode;

public:
    FakeFolder(const FileInfo &fileTemplate, OCC::Vfs::Mode vfsMode = OCC::Vfs::Off);

    void switchToVfs(QSharedPointer<OCC::Vfs> vfs);

    OCC::AccountPtr account() const { return _account; }
    OCC::SyncEngine &syncEngine() const { return *_syncEngine; }
    OCC::SyncJournalDb &syncJournal() const { return *_journalDb; }

    FileModifier &localModifier() { return _localModifier; }
    FileInfo &remoteModifier() { return _fakeAm->currentRemoteState(); }
    FileInfo currentLocalState();

    FileInfo currentRemoteState() { return _fakeAm->currentRemoteState(); }
    FileInfo &uploadState() { return _fakeAm->uploadState(); }
    FileInfo dbState() const;

    struct ErrorList
    {
        FakeAM *_qnam;
        void append(const QString &path, int error = 500)
        {
            _qnam->errorPaths().insert(path, error);
        }
        void clear() { _qnam->errorPaths().clear(); }
    };
    ErrorList serverErrorPaths() { return { _fakeAm }; }
    void setServerOverride(const FakeAM::Override &override) { _fakeAm->setOverride(override); }

    QString localPath() const;

    void scheduleSync();

    void execUntilBeforePropagation();

    void execUntilItemCompleted(const QString &relativePath);

    bool execUntilFinished()
    {
        QSignalSpy spy(_syncEngine.get(), &OCC::SyncEngine::finished);
        bool ok = spy.wait(3600000);
        Q_ASSERT(ok && "Sync timed out");
        return spy[0][0].toBool();
    }

    bool syncOnce()
    {
        scheduleSync();
        return execUntilFinished();
    }

    bool isDehydratedPlaceholder(const QString &filePath);

private:
    void startVfs();
    static void toDisk(QDir &dir, const FileInfo &templateFi);

    void fromDisk(QDir &dir, FileInfo &templateFi);
};


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

struct ItemCompletedSpy : QSignalSpy
{
    explicit ItemCompletedSpy(FakeFolder &folder)
        : QSignalSpy(&folder.syncEngine(), &OCC::SyncEngine::itemCompleted)
    {
    }

    OCC::SyncFileItemPtr findItem(const QString &path) const;
};

// QTest::toString overloads
namespace OCC {
inline char *toString(const SyncFileStatus &s)
{
    return QTest::toString(QStringLiteral("SyncFileStatus(%1)").arg(s.toSocketAPIString()));
}
}

inline void addFiles(QStringList &dest, const FileInfo &fi)
{
    if (fi.isDir) {
        dest += QStringLiteral("%1 - dir").arg(fi.path());
        for (const auto &fi : fi.children)
            addFiles(dest, fi);
    } else {
        dest += QStringLiteral("%1 - %2 %3-bytes (%4)").arg(fi.path(), QString::number(fi.contentSize), QChar::fromLatin1(fi.contentChar), fi.lastModifiedInUtc().toString());
    }
}

inline QString toStringNoElide(const FileInfo &fi)
{
    QStringList files;
    for (const auto &fi : fi.children)
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
        dest += QStringLiteral("%1 - %2 %3 %4").arg(fi.name, fi.isDir ? QStringLiteral("dir") : QStringLiteral("file"), QString::number(fi.lastModifiedInSecondsUTC()), QString::fromUtf8(fi.fileId));
        for (const auto &fi : fi.children)
            addFilesDbData(dest, fi);
    } else {
        dest += QStringLiteral("%1 - %2 %3 %4 %5").arg(fi.name, fi.isDir ? QStringLiteral("dir") : QStringLiteral("file"), QString::number(fi.contentSize), QString::number(fi.lastModifiedInSecondsUTC()), QString::fromUtf8(fi.fileId));
    }
}

inline char *printDbData(const FileInfo &fi)
{
    QStringList files;
    for (const auto &fi : fi.children)
        addFilesDbData(files, fi);
    return QTest::toString(QStringLiteral("FileInfo with %1 files(%2)").arg(files.size()).arg(files.join(QStringLiteral(", "))));
}
