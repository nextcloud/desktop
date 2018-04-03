#ifndef PROPAGATEREMOTEDELETEENCRYPTED_H
#define PROPAGATEREMOTEDELETEENCRYPTED_H

#include <QObject>
#include <QElapsedTimer>

#include "syncfileitem.h"

namespace OCC {

class OwncloudPropagator;
class PropagateRemoteDeleteEncrypted : public QObject
{
    Q_OBJECT
public:
    PropagateRemoteDeleteEncrypted(OwncloudPropagator *_propagator, SyncFileItemPtr item, QObject *parent);

    void start();

signals:
    void finished(bool success);

private:
    void slotFolderEncryptedIdReceived(const QStringList &list);
    void slotTryLock(const QByteArray &folderId);
    void slotFolderLockedSuccessfully(const QByteArray &fileId, const QByteArray &token);
    void slotFolderEncryptedMetadataReceived(const QJsonDocument &json, int statusCode);
    void unlockFolder();
    void taskFailed();

    OwncloudPropagator *_propagator;
    SyncFileItemPtr _item;
    QByteArray _folderToken;
    QByteArray _folderId;
    bool _folderLocked = false;
};

}

#endif // PROPAGATEREMOTEDELETEENCRYPTED_H
