#include "repairfolderencryptionmetadatajob.h"

namespace OCC
{

RepairFolderEncryptionMetadataJob::RepairFolderEncryptionMetadataJob(const AccountPtr &account,
                                                                     SyncJournalDb *journal,
                                                                     const QString &path,
                                                                     const QString &pathNonEncrypted,
                                                                     const QString &remoteSyncRootPath,
                                                                     const QByteArray &fileId,
                                                                     OwncloudPropagator *propagator,
                                                                     SyncFileItemPtr item,
                                                                     QObject *parent)
    : QObject{parent}
    , _account{account}
    , _journal{journal}
    , _path{path}
    , _pathNonEncrypted{pathNonEncrypted}
    , _remoteSyncRootPath{remoteSyncRootPath}
    , _fileId{fileId}
    , _propagator{propagator}
    , _item{item}
{
    SyncJournalFileRecord rec;
    const auto currentPath = !_pathNonEncrypted.isEmpty() ? _pathNonEncrypted : _path;
    const auto currentPathRelative = Utility::fullRemotePathToRemoteSyncRootRelative(currentPath, _remoteSyncRootPath);
    const QString fullRemotePath = Utility::trailingSlashPath(Utility::noLeadingSlashPath(_remoteSyncRootPath)) + currentPathRelative;
    [[maybe_unused]] const auto result = _journal->getRootE2eFolderRecord(Utility::fullRemotePathToRemoteSyncRootRelative(currentPath, _remoteSyncRootPath), &rec);
    _encryptedFolderMetadataHandler.reset(new EncryptedFolderMetadataHandler(account, fullRemotePath, _remoteSyncRootPath, _journal, rec.path()));
}

void RepairFolderEncryptionMetadataJob::start()
{
    connect(_encryptedFolderMetadataHandler.data(), &EncryptedFolderMetadataHandler::uploadFinished,
            this, &RepairFolderEncryptionMetadataJob::metadataUploadFinished);
    connect(_encryptedFolderMetadataHandler.data(), &EncryptedFolderMetadataHandler::fetchFinished,
            this, [this] () {
                _encryptedFolderMetadataHandler->repairMetadata();
                _encryptedFolderMetadataHandler->uploadMetadata();
            });

    _encryptedFolderMetadataHandler->fetchMetadata(EncryptedFolderMetadataHandler::FetchMode::AllowBrokenSignature);
}

QString RepairFolderEncryptionMetadataJob::errorString() const
{
    return _errorString;
}

void RepairFolderEncryptionMetadataJob::metadataUploadFinished()
{
    Q_EMIT finished(static_cast<int>(Status::Success));
}

} // namespace OCC
