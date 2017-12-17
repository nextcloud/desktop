#include "propagateuploadencrypted.h"
#include "clientsideencryptionjobs.h"
#include "networkjobs.h"
#include "clientsideencryption.h"
#include "account.h"

#include <QFileInfo>
#include <QDir>
#include <QUrl>

namespace OCC {

PropagateUploadEncrypted::PropagateUploadEncrypted(OwncloudPropagator *propagator, SyncFileItemPtr item)
: _propagator(propagator),
 _item(item)
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
      QFileInfo info(_item->_file);

      auto getEncryptedStatus = new GetFolderEncryptStatusJob(_propagator->account(),
                                                           info.path());

      connect(getEncryptedStatus, &GetFolderEncryptStatusJob::encryptStatusReceived,
              this, &PropagateUploadEncrypted::slotFolderEncryptedStatusFetched);
      connect(getEncryptedStatus, &GetFolderEncryptStatusJob::encryptStatusError,
             this, &PropagateUploadEncrypted::slotFolderEncryptedStatusError);
      getEncryptedStatus->start();
}

void PropagateUploadEncrypted::slotFolderEncryptedStatusFetched(const QMap<QString, bool>& result)
{
  qDebug() << "####################################";
  qDebug() << "Encrypted Status Result by folder:";
  for(const auto& path : result.keys()) {
    qDebug() << result[path] << path;
  }
  qDebug() << "Uploading to Remote Folder: " << _propagator->_remoteFolder;
  qDebug() << "Uploading from Local Dir" << _propagator->_localDir;
  qDebug() << "Local File" << _item->_file;
  qDebug() << QDir::cleanPath(_propagator->account()->url().path() + QLatin1Char('/')
    + _propagator->account()->davPath() + _propagator->_remoteFolder + _item->_file);
  QFileInfo fileInfo(_item->_file);
  QString currFilePath = fileInfo.path();
  if (!currFilePath.endsWith(QDir::separator()))
    currFilePath += QDir::separator();

  qDebug() << "###################################";
  qDebug() << "Retrieved correctly the encrypted status of the folders." << result;

  /* We are inside an encrypted folder, we need to find it's Id. */
  if (result[currFilePath] == true) {
      QFileInfo info(_item->_file);
      LsColJob *job = new LsColJob(_propagator->account(), info.path(), this);
      job->setProperties({"resourcetype", "http://owncloud.org/ns:fileid"});
      connect(job, &LsColJob::directoryListingSubfolders, this, &PropagateUploadEncrypted::slotFolderEncryptedIdReceived);
      connect(job, &LsColJob::finishedWithError, this, &PropagateUploadEncrypted::slotFolderEncryptedIdError);
      job->start();
  } else {
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
  qDebug() << "Folder" << fileId << "Locked Successfully for Upload";
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
  qDebug() << "Metadata Received" << json.toVariant();

  FolderMetadata metaData(_propagator->account(), json.toJson(QJsonDocument::Compact));

  auto *unlockJob = new UnlockEncryptFolderApiJob(_propagator->account(), _folderId, _folderToken, this);
  connect(unlockJob, &UnlockEncryptFolderApiJob::success, this, &PropagateUploadEncrypted::slotUnlockEncryptedFolderSuccess);
  connect(unlockJob, &UnlockEncryptFolderApiJob::error, this, &PropagateUploadEncrypted::slotUnlockEncryptedFolderError);
  unlockJob->start();
}

void PropagateUploadEncrypted::slotUnlockEncryptedFolderSuccess(const QByteArray& fileId)
{
    qDebug() << "Unlock Job worked for folder " << fileId;
}

void PropagateUploadEncrypted::slotUnlockEncryptedFolderError(const QByteArray& fileId, int httpStatusCode)
{
  qDebug() << "There was an error unlocking " << fileId << httpStatusCode;
}

void PropagateUploadEncrypted::slotFolderLockedError(const QByteArray& fileId, int httpErrorCode)
{
  /* try to call the lock from 5 to 5 seconds
    and fail if it's more than 5 minutes. */
  QTimer::singleShot(5000, this, [this, fileId]{
    if (!_currentLockingInProgress) {
      qDebug() << "Error locking the folder while no other update is locking it up.";
      qDebug() << "Perhaps another client locked it.";
      qDebug() << "Abort";
      return;
    }

    // Perhaps I should remove the elapsed timer if the lock is from this client?
    if (_folderLockFirstTry.elapsed() > /* five minutes */ 1000 * 60 * 5 ) {
      qDebug() << "One minute passed, ignoring more attemps to lock the folder.";
      return;
    }
    slotTryLock(fileId);
  });

  qDebug() << "Folder" << fileId << "Coundn't be locked.";
}

void PropagateUploadEncrypted::slotFolderEncryptedIdError(QNetworkReply *r)
{
  qDebug() << "Error retrieving the Id of the encrypted folder.";
}

void PropagateUploadEncrypted::slotFolderEncryptedStatusError(int error)
{
	qDebug() << "Failed to retrieve the status of the folders." << error;
}

} // namespace OCC
