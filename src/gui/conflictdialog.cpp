/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "conflictdialog.h"
#include "ui_conflictdialog.h"

#include "common/utility.h"
#include "conflictsolver.h"
#include "folderman.h"

#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QPushButton>
#include <QUrl>

namespace {
void forceHeaderFont(QWidget *widget)
{
    auto font = widget->font();
    font.setPointSizeF(font.pointSizeF() * 1.5);
    widget->setFont(font);
}

void setBoldFont(QWidget *widget, bool bold)
{
    auto font = widget->font();
    font.setBold(bold);
    widget->setFont(font);
}
}

namespace OCC {

Q_LOGGING_CATEGORY(lcConflictDialog, "nextcloud.gui.conflictdialog", QtInfoMsg)

ConflictDialog::ConflictDialog(QWidget *parent)
    : QDialog(parent)
    , _ui(new Ui::ConflictDialog)
    , _solver(new ConflictSolver(this))
{
    _ui->setupUi(this);
    forceHeaderFont(_ui->conflictMessage);
    _ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    _ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Keep selected version"));

    _ui->conflictMessage->setTextFormat(Qt::PlainText);

    connect(_ui->localVersionRadio, &QCheckBox::toggled, this, &ConflictDialog::updateButtonStates);
    connect(_ui->localVersionButton, &QToolButton::clicked, this, [=, this] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(_solver->localVersionFilename()));
    });

    connect(_ui->remoteVersionRadio, &QCheckBox::toggled, this, &ConflictDialog::updateButtonStates);
    connect(_ui->remoteVersionButton, &QToolButton::clicked, this, [=, this] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(_solver->remoteVersionFilename()));
    });

    connect(_solver, &ConflictSolver::localVersionFilenameChanged, this, &ConflictDialog::updateWidgets);
    connect(_solver, &ConflictSolver::remoteVersionFilenameChanged, this, &ConflictDialog::updateWidgets);

    connect(_ui->openInFileManagerLink, &QLabel::linkActivated, this, &ConflictDialog::openLocalConflictFolder);
}

QString ConflictDialog::baseFilename() const
{
    return _baseFilename;
}

ConflictDialog::~ConflictDialog() = default;

QString ConflictDialog::localVersionFilename() const
{
    return _solver->localVersionFilename();
}

QString ConflictDialog::remoteVersionFilename() const
{
    return _solver->remoteVersionFilename();
}

void ConflictDialog::setBaseFilename(const QString &baseFilename)
{
    if (_baseFilename == baseFilename) {
        return;
    }

    _baseFilename = baseFilename;
    _ui->conflictMessage->setText(tr("Conflicting versions of %1.").arg(_baseFilename));
}

void ConflictDialog::setLocalVersionFilename(const QString &localVersionFilename)
{
    _solver->setLocalVersionFilename(localVersionFilename);
}

void ConflictDialog::setRemoteVersionFilename(const QString &remoteVersionFilename)
{
    _solver->setRemoteVersionFilename(remoteVersionFilename);
}

void ConflictDialog::accept()
{
    const auto isLocalPicked = _ui->localVersionRadio->isChecked();
    const auto isRemotePicked = _ui->remoteVersionRadio->isChecked();

    Q_ASSERT(isLocalPicked || isRemotePicked);
    if (!isLocalPicked && !isRemotePicked) {
        return;
    }

    const auto solution = isLocalPicked && isRemotePicked ? ConflictSolver::KeepBothVersions
                        : isLocalPicked ? ConflictSolver::KeepLocalVersion
                        : ConflictSolver::KeepRemoteVersion;
    if (_solver->exec(solution)) {
        QDialog::accept();
    }
}

void ConflictDialog::updateWidgets()
{
    QMimeDatabase mimeDb;

    const auto updateGroup = [this, &mimeDb](const QString &filename, QLabel *linkLabel, const QString &linkText, QLabel *mtimeLabel, QLabel *sizeLabel, QToolButton *button) {
        const auto fileUrl = QUrl::fromLocalFile(filename).toString();
        linkLabel->setText(QStringLiteral("<a href=\"%1\">%2</a>").arg(Utility::escape(fileUrl)).arg(linkText));

        const auto info = QFileInfo(filename);
        mtimeLabel->setText(info.lastModified().toString());
        sizeLabel->setText(locale().formattedDataSize(info.size()));

        const auto mime = mimeDb.mimeTypeForFile(filename);
        if (QIcon::hasThemeIcon(mime.iconName())) {
            button->setIcon(QIcon::fromTheme(mime.iconName()));
        } else {
            button->setIcon(QIcon(":/qt-project.org/styles/commonstyle/images/file-128.png"));
        }
    };

    const auto localVersion = _solver->localVersionFilename();
    updateGroup(localVersion,
                _ui->localVersionLink,
                tr("Open local version"),
                _ui->localVersionMtime,
                _ui->localVersionSize,
                _ui->localVersionButton);

    const auto remoteVersion = _solver->remoteVersionFilename();
    updateGroup(remoteVersion,
                _ui->remoteVersionLink,
                tr("Open server version"),
                _ui->remoteVersionMtime,
                _ui->remoteVersionSize,
                _ui->remoteVersionButton);

    const auto localMtime = QFileInfo(localVersion).lastModified();
    const auto remoteMtime = QFileInfo(remoteVersion).lastModified();

    setBoldFont(_ui->localVersionMtime, localMtime > remoteMtime);
    setBoldFont(_ui->remoteVersionMtime, remoteMtime > localMtime);

    const auto localPath = _solver->localVersionFilename();
    const auto folderUrl = QUrl::fromLocalFile(QFileInfo(localPath).absolutePath());

    const auto linkText = Utility::isMac() ? tr("Open in Finder") : Utility::isWindows() ? tr("Open in File Explorer") : tr("Open in file manager");
    _ui->openInFileManagerLink->setStyleSheet(QString());
    const auto linkString = QStringLiteral("<a href=\"%1\">%2</a>").arg(Utility::escape(folderUrl.toString()), linkText);

    _ui->openInFileManagerLink->setText(linkString);
}

void ConflictDialog::updateButtonStates()
{
    const auto isLocalPicked = _ui->localVersionRadio->isChecked();
    const auto isRemotePicked = _ui->remoteVersionRadio->isChecked();
    _ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(isLocalPicked || isRemotePicked);

    const auto text = isLocalPicked && isRemotePicked ? tr("Keep both versions")
                    : isLocalPicked ? tr("Keep local version")
                    : isRemotePicked ? tr("Keep server version")
                    : tr("Keep selected version");
    _ui->buttonBox->button(QDialogButtonBox::Ok)->setText(text);
}

void ConflictDialog::openLocalConflictFolder()
{
    auto localPath = _solver->localVersionFilename();
    QFileInfo fileInfo(localPath);

    if (!localPath.isEmpty() && !fileInfo.exists()) {
        const auto fileName = fileInfo.fileName();

        if (const auto folder = FolderMan::instance()->folderForPath(localPath)) {
            QDirIterator it(folder->path(), {fileName}, QDir::Files, QDirIterator::Subdirectories);
            if (it.hasNext()) {
                localPath = it.next();
                _solver->setLocalVersionFilename(localPath);
                fileInfo.setFile(localPath);
            }
        }
    }

    if (localPath.isEmpty() || !fileInfo.exists() || !QDesktopServices::openUrl(QUrl::fromLocalFile(fileInfo.absolutePath()))) {
        qCWarning(lcConflictDialog) << "Conflict file does not exist at path:" << localPath;

        _ui->openInFileManagerLink->setText(tr("Folder not found"));
        _ui->openInFileManagerLink->setStyleSheet(QStringLiteral("color: #b00020;"));
    }
}

} // namespace OCC
