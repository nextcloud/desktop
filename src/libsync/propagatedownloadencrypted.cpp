#include "propagatedownloadencrypted.h"
#include "clientsideencryptionjobs.h"
#include "encryptedfoldermetadatahandler.h"
#include "foldermetadata.h"

Q_LOGGING_CATEGORY(lcPropagateDownloadEncrypted, "nextcloud.sync.propagator.download.encrypted", QtInfoMsg)


namespace OCC {

PropagateDownloadEncrypted::PropagateDownloadEncrypted(OwncloudPropagator *propagator, const QString &localParentPath, SyncFileItemPtr item, QObject *parent)
    : QObject(parent)
    , _propagator(propagator)
    , _localParentPath(localParentPath)
    , _item(item)
    , _info(_item->_file)
{
    const auto rootPath = Utility::noLeadingSlashPath(_propagator->remotePath());
    const auto remoteFilename = _item->_encryptedFileName.isEmpty() ? _item->_file : _item->_encryptedFileName;
    const auto remotePath = QString(rootPath + remoteFilename);
    _remoteParentPath = remotePath.left(remotePath.lastIndexOf('/'));

    const auto filenameInDb = _item->_file;
    const auto pathInDb = QString(rootPath + filenameInDb);
    const auto parentPathInDb = pathInDb.left(pathInDb.lastIndexOf('/'));
    _parentPathInDb = pathInDb.left(pathInDb.lastIndexOf('/'));
}

void PropagateDownloadEncrypted::start()
{
    SyncJournalFileRecord rec;
    if (!_propagator->_journal->getRootE2eFolderRecord(Utility::fullRemotePathToRemoteSyncRootRelative(_remoteParentPath, _propagator->remotePath()), &rec)
        || !rec.isValid()) {
        emit failed();
        return;
    }
    _encryptedFolderMetadataHandler.reset(new EncryptedFolderMetadataHandler(_propagator->account(), _remoteParentPath, _propagator->remotePath(), _propagator->_journal, rec.path()));

    connect(_encryptedFolderMetadataHandler.data(),
            &EncryptedFolderMetadataHandler::fetchFinished,
            this,
            &PropagateDownloadEncrypted::slotFetchMetadataJobFinished);
    _encryptedFolderMetadataHandler->fetchMetadata(EncryptedFolderMetadataHandler::FetchMode::AllowEmptyMetadata);
}

void PropagateDownloadEncrypted::slotFetchMetadataJobFinished(int statusCode, const QString &message)
{
    if (statusCode != 200) {
        qCCritical(lcPropagateDownloadEncrypted) << "Failed to find encrypted metadata information of remote file" << _info.fileName() << message;
        emit failed();
        return;
    }

    qCDebug(lcPropagateDownloadEncrypted) << "Metadata Received reading" << _item->_instruction << _item->_file << _item->_encryptedFileName;

    const auto metadata = _encryptedFolderMetadataHandler->folderMetadata();

    if (!metadata || !metadata->isValid()) {
        emit failed();
        qCCritical(lcPropagateDownloadEncrypted) << "Failed to find encrypted metadata information of remote file" << _info.fileName();
    }

    const auto files = metadata->files();

    const auto encryptedFilename = _item->_encryptedFileName.section(QLatin1Char('/'), -1);
    for (const FolderMetadata::EncryptedFile &file : files) {
        if (encryptedFilename == file.encryptedFilename) {
            _encryptedInfo = file;

            qCDebug(lcPropagateDownloadEncrypted) << "Found matching encrypted metadata for file, starting download";
            emit fileMetadataFound();
            return;
        }
    }
    qCCritical(lcPropagateDownloadEncrypted) << "Failed to find matching encrypted metadata for file, starting download of remote file" << _info.fileName();
    emit failed();
}

// TODO: Fix this. Exported in the wrong place.
QString createDownloadTmpFileName(const QString &previous);

bool PropagateDownloadEncrypted::decryptFile(QFile& tmpFile)
{
    const QString tmpFileName = createDownloadTmpFileName(_item->_file + QLatin1String("_dec"));
    qCDebug(lcPropagateDownloadEncrypted) << "Content Checksum Computed starting decryption" << tmpFileName;

    tmpFile.close();
    QFile _tmpOutput(_propagator->fullLocalPath(tmpFileName), this);
    EncryptionHelper::fileDecryption(_encryptedInfo.encryptionKey,
                                     _encryptedInfo.initializationVector,
                                     &tmpFile,
                                     &_tmpOutput);

    qCDebug(lcPropagateDownloadEncrypted) << "Decryption finished" << tmpFile.fileName() << _tmpOutput.fileName();

    tmpFile.close();
    _tmpOutput.close();

    // we decripted the temporary into another temporary, so good bye old one
    if (!tmpFile.remove()) {
        qCDebug(lcPropagateDownloadEncrypted) << "Failed to remove temporary file" << tmpFile.errorString();
        _errorString = tmpFile.errorString();
        return false;
    }

    // Let's fool the rest of the logic into thinking this was the actual download
    tmpFile.setFileName(_tmpOutput.fileName());

    return true;
}

QString PropagateDownloadEncrypted::errorString() const
{
  return _errorString;
}

}
