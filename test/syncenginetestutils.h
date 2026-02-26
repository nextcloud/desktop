/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2016 ownCloud GmbH
 * SPDX-License-Identifier: CC0-1.0
 *
 * This software is in the public domain, furnished "as is", without technical
 * support, and with no warranty, express or implied, as to its usefulness for
 * any purpose.
 */
#pragma once

#include "account.h"
#include "common/result.h"
#include "creds/abstractcredentials.h"
#include "logger.h"
#include "filesystem.h"
#include "syncengine.h"
#include "common/syncjournaldb.h"
#include "common/syncjournalfilerecord.h"
#include "common/vfs.h"
#include "csync_exclude.h"
#include "testhelper.h"

#include <QDir>
#include <QNetworkReply>
#include <QMap>
#include <QtTest>

#include <cstring>
#include <memory>

#include <cookiejar.h>
#include <QTimer>

class QJsonDocument;

/*
 * TODO: In theory we should use QVERIFY instead of Q_ASSERT for testing, but this
 * only works when directly called from a QTest :-(
 */


static const QUrl sRootUrl("owncloud://somehost/owncloud/remote.php/dav/");
static const QUrl sRootUrl2("owncloud://somehost/owncloud/remote.php/dav/files/admin/");
static const QUrl sUploadUrl("owncloud://somehost/owncloud/remote.php/dav/uploads/admin/");

inline QString getFilePathFromUrl(const QUrl &url)
{
    QString path = url.path();
    if (path.startsWith(sRootUrl2.path()))
        return path.mid(sRootUrl2.path().length());
    if (path.startsWith(sUploadUrl.path()))
        return path.mid(sUploadUrl.path().length());
    if (path.startsWith(sRootUrl.path()))
        return path.mid(sRootUrl.path().length());
    return {};
}


inline QByteArray generateEtag() {
    return QByteArray::number(QDateTime::currentDateTimeUtc().toMSecsSinceEpoch(), 16) + QByteArray::number(OCC::Utility::rand(), 16);
}
// generates a value as seen in `oc:id` attributes (file ID + instance ID)
inline QByteArray generateFileId() {
    return QByteArray::number(OCC::Utility::rand(), 10) + "oc1x2y3z4w";
}

class PathComponents : public QStringList {
public:
    PathComponents(const char *path);
    PathComponents(const QString &path);
    PathComponents(const QStringList &pathComponents);

    [[nodiscard]] PathComponents parentDirComponents() const;
    [[nodiscard]] PathComponents subComponents() const &;
    PathComponents subComponents() && { removeFirst(); return std::move(*this); }
    [[nodiscard]] QString pathRoot() const { return isEmpty() ? QString{} : first(); }
    [[nodiscard]] QString fileName() const { return isEmpty() ? QString{} : last(); }
};

class FileModifier
{
public:
    enum class LockState {
        FileLocked,
        FileUnlocked,
    };

    virtual ~FileModifier() = default;
    virtual void remove(const QString &relativePath) = 0;
    virtual void insert(const QString &relativePath, qint64 size = 64, char contentChar = 'W') = 0;
    virtual void setContents(const QString &relativePath, char contentChar) = 0;
    virtual void appendByte(const QString &relativePath) = 0;
    virtual void mkdir(const QString &relativePath) = 0;
    virtual void rename(const QString &relativePath, const QString &relativeDestinationDirectory) = 0;
    virtual void setModTime(const QString &relativePath, const QDateTime &modTime) = 0;
    virtual void modifyLockState(const QString &relativePath, LockState lockState, int lockType, const QString &lockOwner, const QString &lockOwnerId, const QString &lockEditorId, quint64 lockTime, quint64 lockTimeout) = 0;
    virtual void setE2EE(const QString &relativepath, const bool enabled) = 0;
};

class DiskFileModifier : public FileModifier
{
    QDir _rootDir;
public:
    DiskFileModifier(const QString &rootDirPath) : _rootDir(rootDirPath) { }
    void remove(const QString &relativePath) override;
    void insert(const QString &relativePath, qint64 size = 64, char contentChar = 'W') override;
    void setContents(const QString &relativePath, char contentChar) override;
    void appendByte(const QString &relativePath) override;

    void mkdir(const QString &relativePath) override;
    void rename(const QString &from, const QString &to) override;
    void setModTime(const QString &relativePath, const QDateTime &modTime) override;
    void modifyLockState(const QString &relativePath, LockState lockState, int lockType, const QString &lockOwner, const QString &lockOwnerId, const QString &lockEditorId, quint64 lockTime, quint64 lockTimeout) override;
    void setE2EE(const QString &relativepath, const bool enabled) override;

    [[nodiscard]] QFile find(const QString &relativePath) const;
};

class FileInfo : public FileModifier
{
public:
    static FileInfo A12_B12_C12_S12();

    FileInfo() = default;
    FileInfo(const QString &name) : name{name} { }
    FileInfo(const QString &name, qint64 size) : name{name}, isDir{false}, size{size} { }
    FileInfo(const QString &name, qint64 size, char contentChar) : name{name}, isDir{false}, size{size}, contentChar{contentChar} { }
    FileInfo(const QString &name, qint64 size, char contentChar, QDateTime mtime) : name{name}, isDir{false}, lastModified(mtime), size{size}, contentChar{contentChar} { }
    FileInfo(const QString &name, const std::initializer_list<FileInfo> &children);

    enum EtagsAction {
        Keep = 0,
        Invalidate,
    };
    struct FolderQuota {
        int64_t bytesUsed = 0;
        int64_t bytesAvailable = 5000000000;

        FolderQuota() : bytesUsed{0}, bytesAvailable{5000000000} {};
        FolderQuota(int64_t bytesUsed, int64_t bytesAvailable)
            : bytesUsed{bytesUsed}
            , bytesAvailable{bytesAvailable}
        {}

        QString bytesAvailableString() const {
            if (_bytesAvailableString.isEmpty()) {
                return QString::number(bytesAvailable);
            }

            return _bytesAvailableString;
        }

        void setBytesAvailableString(const QString &bytesAvailableString) { _bytesAvailableString = bytesAvailableString; }

    private:
        QString _bytesAvailableString = QStringLiteral("");
    };

    void addChild(const FileInfo &info);

    void remove(const QString &relativePath) override;

    void insert(const QString &relativePath, qint64 size = 64, char contentChar = 'W') override;

    void setContents(const QString &relativePath, char contentChar) override;

    void appendByte(const QString &relativePath) override;

    void mkdir(const QString &relativePath) override;

    void rename(const QString &oldPath, const QString &newPath) override;

    void setModTime(const QString &relativePath, const QDateTime &modTime) override;

    void setModTimeKeepEtag(const QString &relativePath, const QDateTime &modTime);

    void setIsLivePhoto(const QString &relativePath, bool isLivePhoto);

    void modifyLockState(const QString &relativePath, LockState lockState, int lockType, const QString &lockOwner, const QString &lockOwnerId, const QString &lockEditorId, quint64 lockTime, quint64 lockTimeout) override;

    void setE2EE(const QString &relativepath, const bool enabled) override;
    void setFolderQuota(const QString &relativePath, const FolderQuota newQuota, const EtagsAction invalidateEtags = EtagsAction::Keep);

    FileInfo *find(PathComponents pathComponents, const EtagsAction invalidateEtags = EtagsAction::Keep);
    FileInfo findRecursive(PathComponents pathComponents, const EtagsAction invalidateEtags = EtagsAction::Keep);

    FileInfo *createDir(const QString &relativePath);

    FileInfo *create(const QString &relativePath, qint64 size, char contentChar);

    bool operator<(const FileInfo &other) const {
        return name < other.name;
    }

    bool operator==(const FileInfo &other) const;

    bool operator!=(const FileInfo &other) const {
        return !operator==(other);
    }

    [[nodiscard]] QString path() const;
    [[nodiscard]] QString absolutePath() const;

    // value of `oc:fileid` from PROPFIND responses, unlike `oc:id` this does
    // not include the instance ID (i.e. only the numbers)
    [[nodiscard]] QByteArray numericFileId() const;

    void fixupParentPathRecursively();

    QString name;
    int operationStatus = 200;
    bool isDir = true;
    bool isShared = false;
    bool downloadForbidden = false;
    OCC::RemotePermissions permissions; // When uset, defaults to everything
    QDateTime lastModified = QDateTime::currentDateTimeUtc().addDays(-7);
    QByteArray etag = generateEtag();
    QByteArray fileId = generateFileId();
    QByteArray checksums;
    QByteArray extraDavProperties;
    qint64 size = 0;
    char contentChar = 'W';
    LockState lockState = LockState::FileUnlocked;
    int lockType = 0;
    QString lockOwner;
    QString lockOwnerId;
    QString lockEditorId;
    quint64 lockTime = 0;
    quint64 lockTimeout = 0;
    bool isEncrypted = false;
    bool isLivePhoto = false;
    FolderQuota folderQuota;

    // Sorted by name to be able to compare trees
    QMap<QString, FileInfo> children;
    QString parentPath;

    FileInfo *findInvalidatingEtags(PathComponents pathComponents);

    friend inline QDebug operator<<(QDebug dbg, const FileInfo& fi) {
        return dbg << "{ " << fi.path() << ": " << fi.children;
    }
};

class FakeReply : public QNetworkReply
{
    Q_OBJECT
public:
    FakeReply(QObject *parent);
    ~FakeReply() override;

    // useful to be public for testing
    using QNetworkReply::setRawHeader;
};

class FakeJsonReply : public FakeReply
{
    Q_OBJECT
public:
    FakeJsonReply(QNetworkAccessManager::Operation op,
                  const QNetworkRequest &request,
                  QObject *parent,
                  int httpReturnCode,
                  const QJsonDocument &reply = QJsonDocument());

    Q_INVOKABLE virtual void respond();

public slots:
    void slotSetFinished();

public:
    void abort() override { }
    qint64 readData(char *buf, qint64 max) override;
    [[nodiscard]] qint64 bytesAvailable() const override;

    QByteArray _body;
};

class FakePropfindReply : public FakeReply
{
    Q_OBJECT
public:
    QByteArray payload;

    explicit FakePropfindReply(FileInfo &remoteRootFileInfo, QNetworkAccessManager::Operation op, const QNetworkRequest &request, QObject *parent);
    explicit FakePropfindReply(const QByteArray &replyContents, QNetworkAccessManager::Operation op, const QNetworkRequest &request, QObject *parent);

    Q_INVOKABLE void respond();

    Q_INVOKABLE void respond404();

    void abort() override { }

    [[nodiscard]] qint64 bytesAvailable() const override;
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

class FakePutMultiFileReply : public FakeReply
{
    Q_OBJECT
public:
    FakePutMultiFileReply(FileInfo &remoteRootFileInfo, QNetworkAccessManager::Operation op, const QNetworkRequest &request, const QString &contentType, const QByteArray &putPayload, const QString &serverVersion, QObject *parent);

    static QVector<FileInfo *> performMultiPart(FileInfo &remoteRootFileInfo, const QNetworkRequest &request, const QByteArray &putPayload, const QString &contentType, const QString &serverVersion);

    Q_INVOKABLE virtual void respond();

    void abort() override;

    [[nodiscard]] qint64 bytesAvailable() const override;
    qint64 readData(char *data, qint64 maxlen) override;

private:
    QVector<FileInfo *> _allFileInfo;

    QByteArray _payload;

    QString _serverVersion;
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
    char payload = 0;
    int size = 0;
    bool aborted = false;

    FakeGetReply(FileInfo &remoteRootFileInfo, QNetworkAccessManager::Operation op, const QNetworkRequest &request, QObject *parent);

    Q_INVOKABLE void respond();

    void abort() override;
    [[nodiscard]] qint64 bytesAvailable() const override;

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
    [[nodiscard]] qint64 bytesAvailable() const override;

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

    FakePayloadReply(QNetworkAccessManager::Operation op, const QNetworkRequest &request,
        const QByteArray &body, int delay, QObject *parent);

    void respond();

    void abort() override {}
    qint64 readData(char *buf, qint64 max) override;
    [[nodiscard]] qint64 bytesAvailable() const override;
    QByteArray _body;

    QMap<QNetworkRequest::KnownHeaders, QByteArray> _additionalHeaders;

    static const int defaultDelay = 10;
};


class FakeErrorReply : public FakeReply
{
    Q_OBJECT
public:
    FakeErrorReply(QNetworkAccessManager::Operation op, const QNetworkRequest &request,
        QObject *parent, int httpErrorCode, const QByteArray &body = QByteArray());

    Q_INVOKABLE virtual void respond();

    // make public to give tests easy interface
    using QNetworkReply::setError;
    using QNetworkReply::setAttribute;

public slots:
    void slotSetFinished();

public:
    void abort() override { }
    qint64 readData(char *buf, qint64 max) override;
    [[nodiscard]] qint64 bytesAvailable() const override;

    QByteArray _body;
};

class FakeJsonErrorReply : public FakeErrorReply
{
    Q_OBJECT
public:
    FakeJsonErrorReply(QNetworkAccessManager::Operation op,
                       const QNetworkRequest &request,
                       QObject *parent,
                       int httpErrorCode,
                       const QJsonDocument &reply = QJsonDocument());
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

class FakeFileLockReply : public FakePropfindReply
{
    Q_OBJECT
public:
    FakeFileLockReply(FileInfo &remoteRootFileInfo,
                      QNetworkAccessManager::Operation op,
                      const QNetworkRequest &request,
                      QObject *parent);
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

    QString _serverVersion = QStringLiteral("10.0.0");

public:
    FakeQNAM(FileInfo initialRoot);
    FileInfo &currentRemoteState() { return _remoteRootFileInfo; }
    FileInfo &uploadState() { return _uploadFileInfo; }

    QHash<QString, int> &errorPaths() { return _errorPaths; }

    void setOverride(const Override &override) { _override = override; }

    QJsonObject forEachReplyPart(QIODevice *outgoingData,
                                 const QString &contentType,
                                 std::function<QJsonObject(const QMap<QString, QByteArray> &)> replyFunction);

    QNetworkReply *overrideReplyWithError(QString fileName, Operation op, QNetworkRequest newRequest);

    void setServerVersion(const QString &version);

protected:
    QNetworkReply *createRequest(Operation op, const QNetworkRequest &request,
        QIODevice *outgoingData = nullptr) override;
};

class FakeCredentials : public OCC::AbstractCredentials
{
    QNetworkAccessManager *_qnam;
    QString _userName = "admin";
public:
    FakeCredentials(QNetworkAccessManager *qnam) : _qnam{qnam} { }
    [[nodiscard]] QString authType() const override { return "test"; }
    [[nodiscard]] QString user() const override { return _userName; }
    [[nodiscard]] QString password() const override { return "password"; }
    [[nodiscard]] QNetworkAccessManager *createQNAM() const override { return _qnam; }
    [[nodiscard]] bool ready() const override { return true; }
    void fetchFromKeychain(const QString &appName) override { Q_UNUSED(appName) }
    void askFromUser() override { }
    bool stillValid(QNetworkReply *reply) override {
        return reply->error() != QNetworkReply::AuthenticationRequiredError;
    }
    void persist() override { }
    void invalidateToken() override { }
    void forgetSensitiveData() override { }
    void setUserName(const QString &userName)
    {
        _userName = userName;
    }
};

class FakeFolder
{
    QTemporaryDir _tempDir;
    QString _tempDirLocalPath;
    DiskFileModifier _localModifier;
    // FIXME: Clarify ownership, double delete
    FakeQNAM *_fakeQnam;
    OCC::AccountPtr _account;
    FakeAccountState *_accountState;
    std::unique_ptr<OCC::SyncJournalDb> _journalDb;
    std::unique_ptr<OCC::SyncEngine> _syncEngine;
    QString _serverVersion = QStringLiteral("10.0.0");

public:
    FakeFolder(const FileInfo &fileTemplate, const OCC::Optional<FileInfo> &localFileInfo = {}, const QString &remotePath = {}, const bool performInitialSync = true);

    void switchToVfs(QSharedPointer<OCC::Vfs> vfs);

    void enableEnforceWindowsFileNameCompatibility();

    void setServerVersion(const QString &version);

    [[nodiscard]] OCC::AccountPtr account() const { return _account; }
    [[nodiscard]] OCC::SyncEngine &syncEngine() const { return *_syncEngine; }
    [[nodiscard]] OCC::SyncJournalDb &syncJournal() const { return *_journalDb; }
    [[nodiscard]] FakeQNAM* networkAccessManager() const { return _fakeQnam; }
    [[nodiscard]] FakeAccountState &accountState() { return *_accountState; }

    DiskFileModifier &localModifier() { return _localModifier; }
    FileInfo &remoteModifier() { return _fakeQnam->currentRemoteState(); }
    FileInfo currentLocalState();

    FileInfo currentRemoteState() { return _fakeQnam->currentRemoteState(); }
    FileInfo &uploadState() { return _fakeQnam->uploadState(); }
    [[nodiscard]] FileInfo dbState() const;

    struct ErrorList {
        FakeQNAM *_qnam;
        void append(const QString &path, int error = 500)
        { _qnam->errorPaths().insert(path, error); }
        void clear() { _qnam->errorPaths().clear(); }
    };
    ErrorList serverErrorPaths() { return {_fakeQnam}; }
    void setServerOverride(const FakeQNAM::Override &override) { _fakeQnam->setOverride(override); }
    QJsonObject forEachReplyPart(QIODevice *outgoingData,
                                 const QString &contentType,
                                 std::function<QJsonObject(const QMap<QString, QByteArray>&)> replyFunction) {
        return _fakeQnam->forEachReplyPart(outgoingData, contentType, replyFunction);
    }

    [[nodiscard]] QString localPath() const;

    void scheduleSync();

    void execUntilBeforePropagation();

    void execUntilItemCompleted(const QString &relativePath);

    bool execUntilFinished() {
        QSignalSpy spy(_syncEngine.get(), &OCC::SyncEngine::finished);
        bool ok = spy.wait(3600000);
        Q_ASSERT(ok && "Sync timed out");
        return spy[0][0].toBool();
    }

    bool syncOnce() {
        scheduleSync();
        return execUntilFinished();
    }

private:
    static void toDisk(QDir &dir, const FileInfo &templateFi);

    static void fromDisk(QDir &dir, FileInfo &templateFi);
};

/* Return the FileInfo for a conflict file for the specified relative filename */
inline const FileInfo *findConflict(FileInfo &dir, const QString &filename)
{
    QFileInfo info(filename);
    const FileInfo *parentDir = dir.find(info.path());
    if (!parentDir)
        return nullptr;
    QString start = info.baseName() + " (conflicted copy";
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

    [[nodiscard]] OCC::SyncFileItemPtr findItem(const QString &path) const;

    [[nodiscard]] OCC::SyncFileItemPtr findItemWithExpectedRank(const QString &path, int rank) const;
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
    return QStringLiteral("FileInfo with %1 files(\n\t%2\n)").arg(files.size()).arg(files.join("\n\t"));
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
            fi.isDir ? "dir" : "file",
            QString::number(fi.lastModified.toSecsSinceEpoch()),
            fi.fileId);
        foreach (const FileInfo &fi, fi.children)
            addFilesDbData(dest, fi);
    } else {
        dest += QStringLiteral("%1 - %2 %3 %4 %5").arg(
            fi.name,
            fi.isDir ? "dir" : "file",
            QString::number(fi.size),
            QString::number(fi.lastModified.toSecsSinceEpoch()),
            fi.fileId);
    }
}

inline char *printDbData(const FileInfo &fi)
{
    QStringList files;
    foreach (const FileInfo &fi, fi.children)
        addFilesDbData(files, fi);
    return QTest::toString(QStringLiteral("FileInfo with %1 files(%2)").arg(files.size()).arg(files.join(", ")));
}
