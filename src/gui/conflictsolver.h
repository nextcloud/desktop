/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef CONFLICTSOLVER_H
#define CONFLICTSOLVER_H

#include <QObject>

class QWidget;

namespace OCC {

class ConflictSolver : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString localVersionFilename READ localVersionFilename WRITE setLocalVersionFilename NOTIFY localVersionFilenameChanged)
    Q_PROPERTY(QString remoteVersionFilename READ remoteVersionFilename WRITE setRemoteVersionFilename NOTIFY remoteVersionFilenameChanged)
    Q_PROPERTY(bool isBulkSolution READ isBulkSolution WRITE setIsBulkSolution NOTIFY isBulkSolutionChanged)
    Q_PROPERTY(bool yesToAllRequested READ yesToAllRequested WRITE setYesToAllRequested NOTIFY yesToAllRequestedChanged)

public:
    enum Solution {
        KeepLocalVersion,
        KeepRemoteVersion,
        KeepBothVersions
    };
    Q_ENUM(Solution);

    explicit ConflictSolver(QWidget *parent = nullptr);

    [[nodiscard]] QString localVersionFilename() const;
    [[nodiscard]] QString remoteVersionFilename() const;
    [[nodiscard]] bool isBulkSolution() const;
    [[nodiscard]] bool yesToAllRequested() const;

    bool exec(Solution solution);

public slots:
    void setLocalVersionFilename(const QString &localVersionFilename);
    void setRemoteVersionFilename(const QString &remoteVersionFilename);
    void setIsBulkSolution(bool isBulkSolution);
    void setYesToAllRequested(bool yesToAllRequested);

signals:
    void localVersionFilenameChanged();
    void remoteVersionFilenameChanged();
    void isBulkSolutionChanged();
    void yesToAllRequestedChanged();

private:
    bool deleteLocalVersion();
    bool renameLocalVersion();
    bool overwriteRemoteVersion();
    bool confirmDeletion();

    QWidget *_parentWidget;
    QString _localVersionFilename;
    QString _remoteVersionFilename;
    bool _isBulkSolution = false;
    bool _yesToAllRequested = false;
};

} // namespace OCC

#endif // CONFLICTSOLVER_H
