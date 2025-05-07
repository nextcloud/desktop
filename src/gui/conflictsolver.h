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

    bool exec(Solution solution);

public slots:
    void setLocalVersionFilename(const QString &localVersionFilename);
    void setRemoteVersionFilename(const QString &remoteVersionFilename);

signals:
    void localVersionFilenameChanged();
    void remoteVersionFilenameChanged();

private:
    bool deleteLocalVersion();
    bool renameLocalVersion();
    bool overwriteRemoteVersion();

    QWidget *_parentWidget;
    QString _localVersionFilename;
    QString _remoteVersionFilename;
};

} // namespace OCC

#endif // CONFLICTSOLVER_H
