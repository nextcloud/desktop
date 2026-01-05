/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
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

bool ConflictSolver::isBulkSolution() const
{
    return _isBulkSolution;
}

bool ConflictSolver::yesToAllRequested() const
{
    return _yesToAllRequested;
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

void ConflictSolver::setIsBulkSolution(bool isBulkSolution)
{
    if (_isBulkSolution == isBulkSolution) {
        return;
    }

    _isBulkSolution = isBulkSolution;
    emit isBulkSolutionChanged();
}

void ConflictSolver::setYesToAllRequested(bool yesToAllRequested)
{
    if (_yesToAllRequested == yesToAllRequested) {
        return;
    }

    _yesToAllRequested = yesToAllRequested;
    emit yesToAllRequestedChanged();
}

bool ConflictSolver::deleteLocalVersion()
{
    if (_localVersionFilename.isEmpty()) {
        return false;
    }

    if (!FileSystem::fileExists(_localVersionFilename)) {
        return false;
    }

    if (!confirmDeletion()) {
        return false;
    }

    if (FileSystem::isDir(_localVersionFilename)) {
        return FileSystem::removeRecursively(_localVersionFilename);
    } else {
        return FileSystem::remove(_localVersionFilename);
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

    const auto renamePattern = [=, this] {
        auto result = QString::fromUtf8(OCC::Utility::conflictFileBaseNameFromPattern(_localVersionFilename.toUtf8()));
        const auto dotIndex = result.lastIndexOf('.');
        return QString(result.left(dotIndex) + "_%1" + result.mid(dotIndex));
    }();

    const auto targetFilename = [=] {
        uint i = 1;
        auto result = renamePattern.arg(i);
        while (FileSystem::fileExists(result)) {
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

    if (!FileSystem::fileExists(_localVersionFilename)) {
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

bool ConflictSolver::confirmDeletion()
{
    if (_yesToAllRequested) {
        return true;
    }

    QMessageBox::StandardButtons buttons = QMessageBox::Yes | QMessageBox::No;
    if (_isBulkSolution) {
        buttons |= QMessageBox::YesToAll;
    }

    QFileInfo info(_localVersionFilename);
    const auto message = FileSystem::isDir(_localVersionFilename)
        ? tr("Do you want to delete the directory <i>%1</i> and all its contents permanently?").arg(Utility::escape(info.dir().dirName()))
                                    : tr("Do you want to delete the file <i>%1</i> permanently?").arg(Utility::escape(info.fileName()));
    const auto result = QMessageBox::question(_parentWidget, tr("Confirm deletion"), message, buttons);
    switch (result)
    {
        case QMessageBox::YesToAll:
            setYesToAllRequested(true);
            return true;
        case QMessageBox::Yes:
            return true;
        default:
            // any other button pressed
            return false;
    }
    return false;
}

} // namespace OCC
