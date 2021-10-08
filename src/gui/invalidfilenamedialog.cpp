/*
 * Copyright (C) by Felix Weilbach <felix.weilbach@nextcloud.com>
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

#include "invalidfilenamedialog.h"
#include "accountfwd.h"
#include "common/syncjournalfilerecord.h"
#include "propagateremotemove.h"
#include "ui_invalidfilenamedialog.h"

#include <folder.h>

#include <QPushButton>
#include <QDir>
#include <qabstractbutton.h>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QPushButton>

#include <array>

namespace {
constexpr std::array<QChar, 9> illegalCharacters({ '\\', '/', ':', '?', '*', '\"', '<', '>', '|' });

QVector<QChar> getIllegalCharsFromString(const QString &string)
{
    QVector<QChar> result;
    for (const auto &character : string) {
        if (std::find(illegalCharacters.begin(), illegalCharacters.end(), character)
            != illegalCharacters.end()) {
            result.push_back(character);
        }
    }
    return result;
}

QString illegalCharacterListToString(const QVector<QChar> &illegalCharacters)
{
    QString illegalCharactersString;
    if (illegalCharacters.size() > 0) {
        illegalCharactersString += illegalCharacters[0];
    }

    for (int i = 1; i < illegalCharacters.count(); ++i) {
        if (illegalCharactersString.contains(illegalCharacters[i])) {
            continue;
        }
        illegalCharactersString += " " + illegalCharacters[i];
    }
    return illegalCharactersString;
}
}

namespace OCC {

InvalidFilenameDialog::InvalidFilenameDialog(AccountPtr account, Folder *folder, QString filePath, QWidget *parent)
    : QDialog(parent)
    , _ui(new Ui::InvalidFilenameDialog)
    , _account(account)
    , _folder(folder)
    , _filePath(std::move(filePath))
{
    Q_ASSERT(_account);
    Q_ASSERT(_folder);

    const auto filePathFileInfo = QFileInfo(_filePath);
    _relativeFilePath = filePathFileInfo.path() + QStringLiteral("/");
    _relativeFilePath = _relativeFilePath.replace(folder->path(), QStringLiteral(""));
    _relativeFilePath = _relativeFilePath.isEmpty() ? QStringLiteral("") : _relativeFilePath + QStringLiteral("/");

    _originalFileName = _relativeFilePath + filePathFileInfo.fileName();

    _ui->setupUi(this);
    _ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    _ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Rename file"));

    _ui->descriptionLabel->setText(tr("The file %1 could not be synced because the name contains characters which are not allowed on this system.").arg(_originalFileName));
    _ui->explanationLabel->setText(tr("The following characters are not allowed on the system: * \" | & ? , ; : \\ / ~ < >"));
    _ui->filenameLineEdit->setText(filePathFileInfo.fileName());

    connect(_ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(_ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(_ui->filenameLineEdit, &QLineEdit::textChanged, this,
        &InvalidFilenameDialog::onFilenameLineEditTextChanged);

    checkIfAllowedToRename();
}

InvalidFilenameDialog::~InvalidFilenameDialog() = default;

void InvalidFilenameDialog::checkIfAllowedToRename()
{
    const auto propfindJob = new PropfindJob(_account, QDir::cleanPath(_folder->remotePath() + _originalFileName));
    propfindJob->setProperties({ "http://owncloud.org/ns:permissions" });
    connect(propfindJob, &PropfindJob::result, this, &InvalidFilenameDialog::onPropfindPermissionSuccess);
    propfindJob->start();
}

void InvalidFilenameDialog::onPropfindPermissionSuccess(const QVariantMap &values)
{
    if (!values.contains("permissions")) {
        return;
    }
    const auto remotePermissions = RemotePermissions::fromServerString(values["permissions"].toString());
    if (!remotePermissions.hasPermission(remotePermissions.CanRename)
        || !remotePermissions.hasPermission(remotePermissions.CanMove)) {
        _ui->errorLabel->setText(
            tr("You don't have the permission to rename this file. Please ask the author of the file to rename it."));
        _ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
        _ui->filenameLineEdit->setEnabled(false);
    }
}

void InvalidFilenameDialog::accept()
{
    _newFilename = _relativeFilePath + _ui->filenameLineEdit->text().trimmed();
    const auto propfindJob = new PropfindJob(_account, QDir::cleanPath(_folder->remotePath() + _newFilename));
    connect(propfindJob, &PropfindJob::result, this, &InvalidFilenameDialog::onRemoteFileAlreadyExists);
    connect(propfindJob, &PropfindJob::finishedWithError, this, &InvalidFilenameDialog::onRemoteFileDoesNotExist);
    propfindJob->start();
}

void InvalidFilenameDialog::onFilenameLineEditTextChanged(const QString &text)
{
    const auto isNewFileNameDifferent = text != _originalFileName;
    const auto illegalContainedCharacters = getIllegalCharsFromString(text);
    const auto containsIllegalChars = !illegalContainedCharacters.empty() || text.endsWith(QLatin1Char('.'));
    const auto isTextValid = isNewFileNameDifferent && !containsIllegalChars;

    if (isTextValid) {
        _ui->errorLabel->setText("");
    } else {
        _ui->errorLabel->setText(tr("Filename contains illegal characters: %1")
                                     .arg(illegalCharacterListToString(illegalContainedCharacters)));
    }

    _ui->buttonBox->button(QDialogButtonBox::Ok)
        ->setEnabled(isTextValid);
}

void InvalidFilenameDialog::onMoveJobFinished()
{
    const auto job = qobject_cast<MoveJob *>(sender());
    const auto error = job->reply()->error();

    if (error != QNetworkReply::NoError) {
        _ui->errorLabel->setText(tr("Could not rename file. Please make sure you are connected to the server."));
        return;
    }

    QDialog::accept();
}

void InvalidFilenameDialog::onRemoteFileAlreadyExists(const QVariantMap &values)
{
    Q_UNUSED(values);

    _ui->errorLabel->setText(tr("Cannot rename file because a file with the same name does already exist on the server. Please pick another name."));
    _ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
}

void InvalidFilenameDialog::onRemoteFileDoesNotExist(QNetworkReply *reply)
{
    Q_UNUSED(reply);

    // File does not exist. We can rename it.
    const auto remoteSource = QDir::cleanPath(_folder->remotePath() + _originalFileName);
    const auto remoteDestionation = QDir::cleanPath(_account->davUrl().path() + _folder->remotePath() + _newFilename);
    const auto moveJob = new MoveJob(_account, remoteSource, remoteDestionation, this);
    connect(moveJob, &MoveJob::finishedSignal, this, &InvalidFilenameDialog::onMoveJobFinished);
    moveJob->start();
}
}
