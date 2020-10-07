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

#include "conflictdialog.h"
#include "ui_conflictdialog.h"

#include "conflictsolver.h"

#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
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

ConflictDialog::ConflictDialog(QWidget *parent)
    : QDialog(parent)
    , _ui(new Ui::ConflictDialog)
    , _solver(new ConflictSolver(this))
{
    _ui->setupUi(this);
    forceHeaderFont(_ui->conflictMessage);
    _ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    _ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Keep selected version"));

    connect(_ui->localVersionRadio, &QCheckBox::toggled, this, &ConflictDialog::updateButtonStates);
    connect(_ui->localVersionButton, &QToolButton::clicked, this, [=] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(_solver->localVersionFilename()));
    });

    connect(_ui->remoteVersionRadio, &QCheckBox::toggled, this, &ConflictDialog::updateButtonStates);
    connect(_ui->remoteVersionButton, &QToolButton::clicked, this, [=] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(_solver->remoteVersionFilename()));
    });

    connect(_solver, &ConflictSolver::localVersionFilenameChanged, this, &ConflictDialog::updateWidgets);
    connect(_solver, &ConflictSolver::remoteVersionFilenameChanged, this, &ConflictDialog::updateWidgets);
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
        linkLabel->setText(QStringLiteral("<a href='%1'>%2</a>").arg(fileUrl).arg(linkText));

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
                tr("Open remote version"),
                _ui->remoteVersionMtime,
                _ui->remoteVersionSize,
                _ui->remoteVersionButton);

    const auto localMtime = QFileInfo(localVersion).lastModified();
    const auto remoteMtime = QFileInfo(remoteVersion).lastModified();

    setBoldFont(_ui->localVersionMtime, localMtime > remoteMtime);
    setBoldFont(_ui->remoteVersionMtime, remoteMtime > localMtime);
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

} // namespace OCC
