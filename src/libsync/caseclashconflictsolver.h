#ifndef CASECLASHCONFLICTSOLVER_H
#define CASECLASHCONFLICTSOLVER_H

#include <QObject>

#include "accountfwd.h"
#include "owncloudlib.h"

class QNetworkReply;

namespace OCC {

class SyncJournalDb;

class OWNCLOUDSYNC_EXPORT CaseClashConflictSolver : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool allowedToRename READ allowedToRename NOTIFY allowedToRenameChanged)

    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)

public:
    explicit CaseClashConflictSolver(const QString &targetFilePath,
                                     const QString &conflictFilePath,
                                     const QString &remotePath,
                                     const QString &localPath,
                                     AccountPtr account,
                                     SyncJournalDb *journal,
                                     QObject *parent = nullptr);

    [[nodiscard]] bool allowedToRename() const;

    [[nodiscard]] QString errorString() const;

signals:
    void allowedToRenameChanged();

    void errorStringChanged();

    void done();

    void failed();

public slots:
    void solveConflict(const QString &newFilename);

    void checkIfAllowedToRename();

private slots:
    void onRemoteDestinationFileAlreadyExists();

    void onRemoteDestinationFileDoesNotExist();

    void onPropfindPermissionSuccess(const QVariantMap &values);

    void onPropfindPermissionError(QNetworkReply *reply);

    void onRemoteSourceFileAlreadyExists();

    void onRemoteSourceFileDoesNotExist();

    void onMoveJobFinished();

private:
    [[nodiscard]] QString remoteNewFilename() const;

    [[nodiscard]] QString remoteTargetFilePath() const;

    void onCheckIfAllowedToRenameComplete(const QVariantMap &values, QNetworkReply *reply = nullptr);

    void processLeadingOrTrailingSpacesError(const QString &fileName);

    AccountPtr _account;

    QString _targetFilePath;

    QString _conflictFilePath;

    QString _newFilename;

    QString _remotePath;

    QString _localPath;

    QString _errorString;

    SyncJournalDb *_journal = nullptr;

    bool _allowedToRename = false;
};

}

#endif // CASECLASHCONFLICTSOLVER_H
