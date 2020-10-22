/*
 * Copyright (C) by Kevin Ottens <kevin.ottens@nextcloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include "conflictsolver.h"

#include <QFileDialog>
#include <QMessageBox>

#include "common/utility.h"
#include "filesystem.h"

namespace OCC {

Q_LOGGING_CATEGORY(lcConflict, "nextcloud.gui.conflictsolver", QtInfoMsg)

ConflictSolver::ConflictSolver(QWidget *parent)
    : QObject(parent)
    , _parentWidget(parent)
{
}

QString ConflictSolver::localVersionFilename() const
{
    return _localVersionFilename;
}

QString ConflictSolver::remoteVersionFilename() const
{
    return _remoteVersionFilename;
}

bool ConflictSolver::exec(ConflictSolver::Solution solution)
{
    switch (solution) {
    case KeepLocalVersion:
        return overwriteRemoteVersion();
    case KeepRemoteVersion:
        return deleteLocalVersion();
    case KeepBothVersions:
        return renameLocalVersion();
    }
    Q_UNREACHABLE();
    return false;
}

void ConflictSolver::setLocalVersionFilename(const QString &localVersionFilename)
{
    if (_localVersionFilename == localVersionFilename) {
        return;
    }

    _localVersionFilename = localVersionFilename;
    emit localVersionFilenameChanged();
}

void ConflictSolver::setRemoteVersionFilename(const QString &remoteVersionFilename)
{
    if (_remoteVersionFilename == remoteVersionFilename) {
        return;
    }

    _remoteVersionFilename = remoteVersionFilename;
    emit remoteVersionFilenameChanged();
}

bool ConflictSolver::deleteLocalVersion()
{
    if (_localVersionFilename.isEmpty()) {
        return false;
    }

    QFileInfo info(_localVersionFilename);
    if (!info.exists()) {
        return false;
    }

    const auto message = info.isDir() ? tr("Do you want to delete the directory <i>%1</i> and all its contents permanently?").arg(info.dir().dirName())
                                      : tr("Do you want to delete the file <i>%1</i> permanently?").arg(info.fileName());
    const auto result = QMessageBox::question(_parentWidget, tr("Confirm deletion"), message, QMessageBox::Yes, QMessageBox::No);
    if (result != QMessageBox::Yes)
        return false;

    if (info.isDir()) {
        return FileSystem::removeRecursively(_localVersionFilename);
    } else {
        return QFile(_localVersionFilename).remove();
    }
}

bool ConflictSolver::renameLocalVersion()
{
    if (_localVersionFilename.isEmpty()) {
        return false;
    }

    QFileInfo info(_localVersionFilename);
    if (!info.exists()) {
        return false;
    }

    const auto renamePattern = [=] {
        auto result = QString::fromUtf8(OCC::Utility::conflictFileBaseNameFromPattern(_localVersionFilename.toUtf8()));
        const auto dotIndex = result.lastIndexOf('.');
        return QString(result.left(dotIndex) + "_%1" + result.mid(dotIndex));
    }();

    const auto targetFilename = [=] {
        uint i = 1;
        auto result = renamePattern.arg(i);
        while (QFileInfo::exists(result)) {
            Q_ASSERT(i > 0);
            i++;
            result = renamePattern.arg(i);
        }
        return result;
    }();

    QString error;
    if (FileSystem::uncheckedRenameReplace(_localVersionFilename, targetFilename, &error)) {
        return true;
    } else {
        qCWarning(lcConflict) << "Rename error:" << error;
        QMessageBox::warning(_parentWidget, tr("Error"), tr("Moving file failed:\n\n%1").arg(error));
        return false;
    }
}

bool ConflictSolver::overwriteRemoteVersion()
{
    if (_localVersionFilename.isEmpty()) {
        return false;
    }

    if (_remoteVersionFilename.isEmpty()) {
        return false;
    }

    QFileInfo info(_localVersionFilename);
    if (!info.exists()) {
        return false;
    }

    QString error;
    if (FileSystem::uncheckedRenameReplace(_localVersionFilename, _remoteVersionFilename, &error)) {
        return true;
    } else {
        qCWarning(lcConflict) << "Rename error:" << error;
        QMessageBox::warning(_parentWidget, tr("Error"), tr("Moving file failed:\n\n%1").arg(error));
        return false;
    }
}

} // namespace OCC
