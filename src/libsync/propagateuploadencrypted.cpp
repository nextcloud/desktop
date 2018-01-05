#include "propagateuploadencrypted.h"
#include "clientsideencryptionjobs.h"
#include "networkjobs.h"
#include "clientsideencryption.h"
#include "account.h"

#include <QFileInfo>
#include <QDir>
#include <QUrl>
#include <QFile>
#include <QTemporaryFile>
#include <QLoggingCategory>

namespace OCC {

Q_DECLARE_LOGGING_CATEGORY(lcPropagateUpload)

PropagateUploadEncrypted::PropagateUploadEncrypted(OwncloudPropagator *propagator, SyncFileItemPtr item)
: _propagator(propagator),
 _item(item),
 _metadata(nullptr)
{
}

void PropagateUploadEncrypted::start()
{
  /* If the file is in a encrypted-enabled nextcloud instance, we need to
      * do the long road: Fetch the folder status of the encrypted bit,
      * if it's encrypted, find the ID of the folder.
      * lock the folder using it's id.
      * download the metadata
      * update the metadata
      * upload the file
      * upload the metadata
      * unlock the folder.
      *
      * If the folder is unencrypted we just follow the old way.
      */
      qCDebug(lcPropagateUpload) << "Starting to send an encrypted file!";
      QFileInfo info(_item->_file);
      auto getEncryptedStatus = new GetFolderEncryptStatusJob(_propagator->account(),
                                                           info.path());

      connect(getEncryptedStatus, &GetFolderEncryptStatusJob::encryptStatusFolderReceived,
              this, &PropagateUploadEncrypted::slotFolderEncryptedStatusFetched);
      connect(getEncryptedStatus, &GetFolderEncryptStatusJob::encryptStatusError,
             this, &PropagateUploadEncrypted::slotFolderEncryptedStatusError);
      getEncryptedStatus->start();
}

void PropagateUploadEncrypted::slotFolderEncryptedStatusFetched(const QString &folder, bool isEncrypted)
{
  qCDebug(lcPropagateUpload) << "Encrypted Status Fetched" << folder << isEncrypted;

  /* We are inside an encrypted folder, we need to find it's Id. */
  if (isEncrypted) {
      qCDebug(lcPropagateUpload) << "Folder is encrypted, let's get the Id from it.";
      auto job = new LsColJob(_propagator->account(), folder, this);
      job->setProperties({"resourcetype", "http://owncloud.org/ns:fileid"});
      connect(job, &LsColJob::directoryListingSubfolders, this, &PropagateUploadEncrypted::slotFolderEncryptedIdReceived);
      connect(job, &LsColJob::finishedWithError, this, &PropagateUploadEncrypted::slotFolderEncryptedIdError);
      job->start();
  } else {
    qCDebug(lcPropagateUpload) << "Folder is not encrypted, getting back to default.";
    emit folerNotEncrypted();
  }
}



/* We try to lock a folder, if it's locked we try again in one second.
 * if it's still locked we try again in one second. looping untill one minute.
 *                                                                      -> fail.
 * the 'loop':                                                         /
 *    slotFolderEncryptedIdReceived -> slotTryLock -> lockError -> stillTime? -> slotTryLock
 *                                        \
 *                                         -> success.
 */

void PropagateUploadEncrypted::slotFolderEncryptedIdReceived(const QStringList &list)
{
  qCDebug(lcPropagateUpload) << "Received id of folder, trying to lock it so we can prepare the metadata";
  auto job = qobject_cast<LsColJob *>(sender());
  const auto& folderInfo = job->_folderInfos.value(list.first());
  _folderLockFirstTry.start();
  slotTryLock(folderInfo.fileId);
}

void PropagateUploadEncrypted::slotTryLock(const QByteArray& fileId)
{
  auto *lockJob = new LockEncryptFolderApiJob(_propagator->account(), fileId, this);
  connect(lockJob, &LockEncryptFolderApiJob::success, this, &PropagateUploadEncrypted::slotFolderLockedSuccessfully);
  connect(lockJob, &LockEncryptFolderApiJob::error, this, &PropagateUploadEncrypted::slotFolderLockedError);
  lockJob->start();
}

void PropagateUploadEncrypted::slotFolderLockedSuccessfully(const QByteArray& fileId, const QByteArray& token)
{
  qCDebug(lcPropagateUpload) << "Folder" << fileId << "Locked Successfully for Upload, Fetching Metadata";
  // Should I use a mutex here?
  _currentLockingInProgress = true;
  _folderToken = token;
  _folderId = fileId;

  auto job = new GetMetadataApiJob(_propagator->account(), _folderId);
  connect(job, &GetMetadataApiJob::jsonReceived,
          this, &PropagateUploadEncrypted::slotFolderEncriptedMetadataReceived);
  job->start();
}

void PropagateUploadEncrypted::slotFolderEncriptedMetadataReceived(const QJsonDocument &json, int statusCode)
{
  qCDebug(lcPropagateUpload) << "Metadata Received, Preparing it for the new file." << json.toVariant();

  // Encrypt File!
  _metadata = new FolderMetadata(_propagator->account(), json.toJson(QJsonDocument::Compact));

  QFileInfo info(_propagator->_localDir + QDir::separator() + _item->_file);
  qCDebug(lcPropagateUpload) << "Creating the encrypted file metadata helper.";

  EncryptedFile encryptedFile;
  encryptedFile.authenticationTag = "NOISE"; // TODO: Remove the noise.
  encryptedFile.encryptedFilename = EncryptionHelper::generateRandomString(20);
  encryptedFile.encryptionKey = EncryptionHelper::generateRandom(16);
  encryptedFile.fileVersion = 1;
  encryptedFile.initializationVector = EncryptionHelper::generateRandom(16);
  encryptedFile.metadataKey = 1;
  encryptedFile.originalFilename = info.fileName();
  _metadata->addEncryptedFile(encryptedFile);
  _encryptedFile = encryptedFile;

  qCDebug(lcPropagateUpload) << "Metadata created, sending to the server.";
  auto job = new UpdateMetadataApiJob(_propagator->account(),
                                      _folderId,
                                      _metadata->encryptedMetadata(),
                                      _folderToken);

  connect(job, &UpdateMetadataApiJob::success, this, &PropagateUploadEncrypted::slotUpdateMetadataSuccess);
  connect(job, &UpdateMetadataApiJob::error, this, &PropagateUploadEncrypted::slotUpdateMetadataError);
  job->start();
}

void PropagateUploadEncrypted::slotUpdateMetadataSuccess(const QByteArray& fileId)
{
  qCDebug(lcPropagateUpload) << "Uploading of the metadata success, Encrypting the file";
  QFileInfo info(_propagator->_localDir + QDir::separator() + _item->_file);
  auto *input = new QFile(info.absoluteFilePath());
  auto *output = new QFile(QDir::tempPath() + QDir::separator() + _encryptedFile.encryptedFilename);
  EncryptionHelper::fileEncryption(_encryptedFile.encryptionKey,
                                  _encryptedFile.initializationVector,
                                  input, output);

  // File is Encrypted, Upload it.
  QFileInfo outputInfo(output->fileName());
  input->deleteLater();
  output->deleteLater();

  qCDebug(lcPropagateUpload) << "Encrypted Info:" << outputInfo.path() << outputInfo.fileName() << outputInfo.size();
  qCDebug(lcPropagateUpload) << "Finalizing the upload part, now the actuall uploader will take over";
  emit finalized(outputInfo.path() + QLatin1Char('/') + outputInfo.fileName(),
                 _item->_file.section(QLatin1Char('/'), 0, -2) + QLatin1Char('/') + outputInfo.fileName(),
                 outputInfo.size());
}

void PropagateUploadEncrypted::slotUpdateMetadataError(const QByteArray& fileId, int httpErrorResponse)
{
  qCDebug(lcPropagateUpload) << "Update metadata error for folder" << fileId << "with error" << httpErrorResponse;
}

void PropagateUploadEncrypted::slotUnlockEncryptedFolderSuccess(const QByteArray& fileId)
{
    qCDebug(lcPropagateUpload) << "Unlock Job worked for folder " << fileId;
}

void PropagateUploadEncrypted::slotUnlockEncryptedFolderError(const QByteArray& fileId, int httpStatusCode)
{
  qCDebug(lcPropagateUpload) << "There was an error unlocking " << fileId << httpStatusCode;
}

void PropagateUploadEncrypted::slotFolderLockedError(const QByteArray& fileId, int httpErrorCode)
{
  /* try to call the lock from 5 to 5 seconds
    and fail if it's more than 5 minutes. */
  QTimer::singleShot(5000, this, [this, fileId]{
    if (!_currentLockingInProgress) {
      qCDebug(lcPropagateUpload) << "Error locking the folder while no other update is locking it up.";
      qCDebug(lcPropagateUpload) << "Perhaps another client locked it.";
      qCDebug(lcPropagateUpload) << "Abort";
      return;
    }

    // Perhaps I should remove the elapsed timer if the lock is from this client?
    if (_folderLockFirstTry.elapsed() > /* five minutes */ 1000 * 60 * 5 ) {
      qCDebug(lcPropagateUpload) << "One minute passed, ignoring more attemps to lock the folder.";
      return;
    }
    slotTryLock(fileId);
  });

  qCDebug(lcPropagateUpload) << "Folder" << fileId << "Coundn't be locked.";
}

void PropagateUploadEncrypted::slotFolderEncryptedIdError(QNetworkReply *r)
{
  qCDebug(lcPropagateUpload) << "Error retrieving the Id of the encrypted folder.";
}

void PropagateUploadEncrypted::slotFolderEncryptedStatusError(int error)
{
    qCDebug(lcPropagateUpload) << "Failed to retrieve the status of the folders." << error;
}

} // namespace OCC
