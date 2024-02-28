#include "propagateuploadencrypted.h"
#include "clientsideencryptionjobs.h"
#include "networkjobs.h"
#include "clientsideencryption.h"
#include "foldermetadata.h"
#include "encryptedfoldermetadatahandler.h"
#include "account.h"
#include <QFileInfo>
#include <QDir>
#include <QUrl>
#include <QFile>
#include <QTemporaryFile>
#include <QLoggingCategory>
#include <QMimeDatabase>

namespace OCC {

Q_LOGGING_CATEGORY(lcPropagateUploadEncrypted, "nextcloud.sync.propagator.upload.encrypted", QtInfoMsg)

PropagateUploadEncrypted::PropagateUploadEncrypted(OwncloudPropagator *propagator, const QString &remoteParentPath, SyncFileItemPtr item, QObject *parent)
    : QObject(parent)
    , _propagator(propagator)
    , _remoteParentPath(remoteParentPath)
    , _item(item)
{
    const auto rootPath = [=]() {
        const auto result = _propagator->remotePath();
        if (result.startsWith('/')) {
            return result.mid(1);
        } else {
            return result;
        }
    }();
    _remoteParentAbsolutePath = [=] {
        auto path = QString(rootPath + _remoteParentPath);
        if (path.endsWith('/')) {
            path.chop(1);
        }
        return path;
    }();
}


void PropagateUploadEncrypted::start()
{
    /* If the file is in a encrypted folder, which we know, we wouldn't be here otherwise,
     * we need to do the long road:
     * find the ID of the folder.
     * lock the folder using it's id.
     * download the metadata
     * update the metadata
     * upload the file
     * upload the metadata
     * unlock the folder.
     */
    // Encrypt File!
    SyncJournalFileRecord rec;
    if (!_propagator->_journal->getRootE2eFolderRecord(Utility::fullRemotePathToRemoteSyncRootRelative(_remoteParentAbsolutePath, _propagator->remotePath()),
                                                       &rec)
        || !rec.isValid()) {
        emit error();
        return;
    }
    _encryptedFolderMetadataHandler.reset(new EncryptedFolderMetadataHandler(_propagator->account(),
                                                                                       _remoteParentAbsolutePath,
                                                                                       _propagator->_journal,
                                                                                       rec.path()));

    connect(_encryptedFolderMetadataHandler.data(), &EncryptedFolderMetadataHandler::fetchFinished,
        this, &PropagateUploadEncrypted::slotFetchMetadataJobFinished);
    _encryptedFolderMetadataHandler->fetchMetadata(EncryptedFolderMetadataHandler::FetchMode::AllowEmptyMetadata);
}

void PropagateUploadEncrypted::unlockFolder()
{
    connect(_encryptedFolderMetadataHandler.data(), &EncryptedFolderMetadataHandler::folderUnlocked, this, &PropagateUploadEncrypted::folderUnlocked);
    _encryptedFolderMetadataHandler->unlockFolder();
}

bool PropagateUploadEncrypted::isUnlockRunning() const
{
    return _encryptedFolderMetadataHandler->isUnlockRunning();
}

bool PropagateUploadEncrypted::isFolderLocked() const
{
    return _encryptedFolderMetadataHandler->isFolderLocked();
}

const QByteArray PropagateUploadEncrypted::folderToken() const
{
    return _encryptedFolderMetadataHandler ? _encryptedFolderMetadataHandler->folderToken() : QByteArray{};
}

void PropagateUploadEncrypted::slotFetchMetadataJobFinished(int statusCode, const QString &message)
{
    qCDebug(lcPropagateUploadEncrypted) << "Metadata Received, Preparing it for the new file." << message;

    if (statusCode != 200) {
        emit error();
        return;
    }

    if (!_encryptedFolderMetadataHandler->folderMetadata() || !_encryptedFolderMetadataHandler->folderMetadata()->isValid()) {
        qCDebug(lcPropagateUploadEncrypted()) << "There was an error encrypting the file, aborting upload. Invalid metadata.";
        emit error();
        return;
    }

    
    const auto metadata = _encryptedFolderMetadataHandler->folderMetadata();

    QFileInfo info(_propagator->fullLocalPath(_item->_file));
    const QString fileName = info.fileName();

    // Find existing metadata for this file
    bool found = false;
    FolderMetadata::EncryptedFile encryptedFile;
    const QVector<FolderMetadata::EncryptedFile> files = metadata->files();

    for (const FolderMetadata::EncryptedFile &file : files) {
        if (file.originalFilename == fileName) {
            encryptedFile = file;
            found = true;
        }
    }

    // New encrypted file so set it all up!
    if (!found) {
        encryptedFile.encryptionKey = EncryptionHelper::generateRandom(16);
        encryptedFile.encryptedFilename = EncryptionHelper::generateRandomFilename();
        encryptedFile.originalFilename = fileName;

        QMimeDatabase mdb;
        encryptedFile.mimetype = mdb.mimeTypeForFile(info).name().toLocal8Bit();

        // Other clients expect "httpd/unix-directory" instead of "inode/directory"
        // Doesn't matter much for us since we don't do much about that mimetype anyway
        if (encryptedFile.mimetype == QByteArrayLiteral("inode/directory")) {
            encryptedFile.mimetype = QByteArrayLiteral("httpd/unix-directory");
        }
    }

    encryptedFile.initializationVector = EncryptionHelper::generateRandom(16);

    _item->_encryptedFileName = _remoteParentPath + QLatin1Char('/') + encryptedFile.encryptedFilename;
    _item->_e2eEncryptionStatusRemote = metadata->existingMetadataEncryptionStatus();
    _item->_e2eEncryptionServerCapability =
        EncryptionStatusEnums::fromEndToEndEncryptionApiVersion(_propagator->account()->capabilities().clientSideEncryptionVersion());

    qCDebug(lcPropagateUploadEncrypted) << "Creating the encrypted file.";

    if (info.isDir()) {
        _completeFileName = encryptedFile.encryptedFilename;
    } else {
        QFile input(info.absoluteFilePath());
        QFile output(QDir::tempPath() + QDir::separator() + encryptedFile.encryptedFilename);

        QByteArray tag;
        bool encryptionResult = EncryptionHelper::fileEncryption(encryptedFile.encryptionKey, encryptedFile.initializationVector, &input, &output, tag);

        if (!encryptionResult) {
            qCDebug(lcPropagateUploadEncrypted()) << "There was an error encrypting the file, aborting upload.";
            emit error();
            return;
        }

        encryptedFile.authenticationTag = tag;
        _completeFileName = output.fileName();
    }

    qCDebug(lcPropagateUploadEncrypted) << "Creating the metadata for the encrypted file.";

    metadata->addEncryptedFile(encryptedFile);

    qCDebug(lcPropagateUploadEncrypted) << "Metadata created, sending to the server.";

    connect(_encryptedFolderMetadataHandler.data(), &EncryptedFolderMetadataHandler::uploadFinished, this, &PropagateUploadEncrypted::slotUploadMetadataFinished);
    _encryptedFolderMetadataHandler->uploadMetadata(EncryptedFolderMetadataHandler::UploadMode::KeepLock);
}

void PropagateUploadEncrypted::slotUploadMetadataFinished(int statusCode, const QString &message)
{
    if (statusCode != 200) {
        qCDebug(lcPropagateUploadEncrypted) << "Update metadata error for folder" << _encryptedFolderMetadataHandler->folderId() << "with error" << message;
        qCDebug(lcPropagateUploadEncrypted()) << "Unlocking the folder.";
        emit error();
        return;
    }

    qCDebug(lcPropagateUploadEncrypted) << "Uploading of the metadata success, Encrypting the file";
    QFileInfo outputInfo(_completeFileName);

    qCDebug(lcPropagateUploadEncrypted) << "Encrypted Info:" << outputInfo.path() << outputInfo.fileName() << outputInfo.size();
    qCDebug(lcPropagateUploadEncrypted) << "Finalizing the upload part, now the actuall uploader will take over";
    emit finalized(outputInfo.path() + QLatin1Char('/') + outputInfo.fileName(),
                   _remoteParentPath + QLatin1Char('/') + outputInfo.fileName(),
                   outputInfo.size());
}

} // namespace OCC