#ifndef PROPAGATEUPLOADENCRYPTED_H
#define PROPAGATEUPLOADENCRYPTED_H

#include <QObject>
#include <QString>
#include <QMap>
#include <QByteArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QScopedPointer>
#include <QFile>
#include <QTemporaryFile>

#include "owncloudpropagator.h"
#include "clientsideencryption.h"

namespace OCC {
class FolderMetadata;

  /* This class is used if the server supports end to end encryption.
 * It will fire for *any* folder, encrypted or not, because when the
 * client starts the upload request we don't know if the folder is
 * encrypted on the server.
 *
 * emits:
 * finalized() if the encrypted file is ready to be uploaded
 * error() if there was an error with the encryption
 * folderNotEncrypted() if the file is within a folder that's not encrypted.
 *
 */

class PropagateUploadEncrypted : public QObject
{
  Q_OBJECT
public:
    PropagateUploadEncrypted(OwncloudPropagator *propagator, const QString &remoteParentPath, SyncFileItemPtr item, QObject *parent = nullptr);
    ~PropagateUploadEncrypted() override = default;

    void start();

    void unlockFolder();

    [[nodiscard]] bool isUnlockRunning() const { return _isUnlockRunning; }
    [[nodiscard]] bool isFolderLocked() const { return _isFolderLocked; }
    [[nodiscard]] const QByteArray folderToken() const { return _folderToken; }

private slots:
    void slotFolderEncryptedIdReceived(const QStringList &list);
    void slotFolderEncryptedIdError(QNetworkReply *r);
    void slotFolderLockedSuccessfully(const QByteArray& fileId, const QByteArray& token);
    void slotFolderLockedError(const QByteArray& fileId, int httpErrorCode);
    void slotTryLock(const QByteArray& fileId);
    void slotFolderEncryptedMetadataReceived(const QJsonDocument &json, int statusCode);
    void slotFolderEncryptedMetadataError(const QByteArray& fileId, int httpReturnCode);
    void slotUpdateMetadataSuccess(const QByteArray& fileId);
    void slotUpdateMetadataError(const QByteArray& fileId, int httpReturnCode);

signals:
    // Emitted after the file is encrypted and everything is setup.
    void finalized(const QString& path, const QString& filename, quint64 size);
    void error();
    void folderUnlocked(const QByteArray &folderId, int httpStatus);

private:
  OwncloudPropagator *_propagator;
  QString _remoteParentPath;
  SyncFileItemPtr _item;

  QByteArray _folderToken;
  QByteArray _folderId;

  QElapsedTimer _folderLockFirstTry;
  bool _currentLockingInProgress = false;

  bool _isUnlockRunning = false;
  bool _isFolderLocked = false;

  QByteArray _generatedKey;
  QByteArray _generatedIv;
  QScopedPointer<FolderMetadata> _metadata;
  EncryptedFile _encryptedFile;
  QString _completeFileName;
};


}
#endif
