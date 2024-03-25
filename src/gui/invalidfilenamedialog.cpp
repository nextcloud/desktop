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

#include "filesystem.h"
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

InvalidFilenameDialog::InvalidFilenameDialog(AccountPtr account, Folder *folder, QString filePath, FileLocation fileLocation, QWidget *parent)
    : QDialog(parent)
    , _ui(new Ui::InvalidFilenameDialog)
    , _account(account)
    , _folder(folder)
    , _filePath(std::move(filePath))
    , _fileLocation(fileLocation)
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

    _ui->descriptionLabel->setTextFormat(Qt::PlainText);
    _ui->errorLabel->setTextFormat(Qt::PlainText);

    _ui->descriptionLabel->setText(tr("The file \"%1\" could not be synced because the name contains characters which are not allowed on this system.").arg(_originalFileName));
    _ui->explanationLabel->setText(tr("The following characters are not allowed on the system: * \" | & ? , ; : \\ / ~ < > leading/trailing spaces"));
    _ui->filenameLineEdit->setText(filePathFileInfo.fileName());

    connect(_ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(_ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    _ui->errorLabel->setText(
        tr("Checking rename permissions …"));
    _ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    _ui->filenameLineEdit->setEnabled(false);

    connect(_ui->filenameLineEdit, &QLineEdit::textChanged, this,
        &InvalidFilenameDialog::onFilenameLineEditTextChanged);

    if (_fileLocation == FileLocation::NewLocalFile) {
        allowRenaming();
        _ui->errorLabel->setText({});
    } else {
        checkIfAllowedToRename();
    }
}

InvalidFilenameDialog::~InvalidFilenameDialog() = default;

void InvalidFilenameDialog::checkIfAllowedToRename()
{
    const auto propfindJob = new PropfindJob(_account, QDir::cleanPath(_folder->remotePath() + _originalFileName));
    propfindJob->setProperties({"http://owncloud.org/ns:permissions", "http://nextcloud.org/ns:is-mount-root"});
    connect(propfindJob, &PropfindJob::result, this, &InvalidFilenameDialog::onPropfindPermissionSuccess);
    connect(propfindJob, &PropfindJob::finishedWithError, this, &InvalidFilenameDialog::onPropfindPermissionError);
    propfindJob->start();
}

void InvalidFilenameDialog::onCheckIfAllowedToRenameComplete(const QVariantMap &values, QNetworkReply *reply)
{
    const auto isAllowedToRename = [](const RemotePermissions remotePermissions) {
        return remotePermissions.hasPermission(remotePermissions.CanRename)
            && remotePermissions.hasPermission(remotePermissions.CanMove);
    };

    if (values.contains("permissions") && !isAllowedToRename(RemotePermissions::fromServerString(values["permissions"].toString()))) {
        _ui->errorLabel->setText(
            tr("You don't have the permission to rename this file. Please ask the author of the file to rename it."));
        return;
    } else if (reply) {
        if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 404) {
            _ui->errorLabel->setText(
                tr("Failed to fetch permissions with error %1").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()));
            return;
        }
    }

    allowRenaming();
}

bool InvalidFilenameDialog::processLeadingOrTrailingSpacesError(const QString &fileName)
{
    const auto hasLeadingSpaces = fileName.startsWith(QLatin1Char(' '));
    const auto hasTrailingSpaces = fileName.endsWith(QLatin1Char(' '));

    _ui->buttonBox->setStandardButtons(_ui->buttonBox->standardButtons() &~ QDialogButtonBox::No);

    if (hasLeadingSpaces || hasTrailingSpaces) {
        if (hasLeadingSpaces && hasTrailingSpaces) {
            _ui->errorLabel->setText(tr("Filename contains leading and trailing spaces."));
        }
        else if (hasLeadingSpaces) {
            _ui->errorLabel->setText(tr("Filename contains leading spaces."));
        } else if (hasTrailingSpaces) {
            _ui->errorLabel->setText(tr("Filename contains trailing spaces."));
        }

        if (!Utility::isWindows()) {
            _ui->buttonBox->setStandardButtons(_ui->buttonBox->standardButtons() | QDialogButtonBox::No);
            _ui->buttonBox->button(QDialogButtonBox::No)->setText(tr("Use invalid name"));
            connect(_ui->buttonBox->button(QDialogButtonBox::No), &QPushButton::clicked, this, &InvalidFilenameDialog::useInvalidName);
        }

        return true;
    }

    return false;
}

void InvalidFilenameDialog::onPropfindPermissionSuccess(const QVariantMap &values)
{
    onCheckIfAllowedToRenameComplete(values);
}

void InvalidFilenameDialog::onPropfindPermissionError(QNetworkReply *reply)
{
    onCheckIfAllowedToRenameComplete({}, reply);
}

void InvalidFilenameDialog::allowRenaming()
{
    _ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
    _ui->filenameLineEdit->setEnabled(true);
    _ui->filenameLineEdit->selectAll();

    const auto filePathFileInfo = QFileInfo(_filePath);
    const auto fileName = filePathFileInfo.fileName();
    processLeadingOrTrailingSpacesError(fileName);
}

void InvalidFilenameDialog::useInvalidName()
{
    emit acceptedInvalidName(_filePath);
}

void InvalidFilenameDialog::accept()
{
    _newFilename = _relativeFilePath + _ui->filenameLineEdit->text().trimmed();
    const auto propfindJob = new PropfindJob(_account, QDir::cleanPath(_folder->remotePath() + _newFilename));
    connect(propfindJob, &PropfindJob::result, this, &InvalidFilenameDialog::onRemoteDestinationFileAlreadyExists);
    connect(propfindJob, &PropfindJob::finishedWithError, this, &InvalidFilenameDialog::onRemoteDestinationFileDoesNotExist);
    propfindJob->start();
}

void InvalidFilenameDialog::onFilenameLineEditTextChanged(const QString &text)
{
    const auto isNewFileNameDifferent = text != _originalFileName;
    const auto illegalContainedCharacters = getIllegalCharsFromString(text);
    const auto containsIllegalChars = !illegalContainedCharacters.empty() || text.endsWith(QLatin1Char('.'));
    const auto isTextValid = isNewFileNameDifferent && !containsIllegalChars;

    _ui->errorLabel->setText("");

    if (!processLeadingOrTrailingSpacesError(text) && !isTextValid){
        _ui->errorLabel->setText(tr("Filename contains illegal characters: %1").arg(illegalCharacterListToString(illegalContainedCharacters)));
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

void InvalidFilenameDialog::onRemoteDestinationFileAlreadyExists(const QVariantMap &values)
{
    Q_UNUSED(values);

    _ui->errorLabel->setText(tr("Cannot rename file because a file with the same name does already exist on the server. Please pick another name."));
    _ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
}

void InvalidFilenameDialog::onRemoteDestinationFileDoesNotExist(QNetworkReply *reply)
{
    Q_UNUSED(reply);

    const auto propfindJob = new PropfindJob(_account, QDir::cleanPath(_folder->remotePath() + _originalFileName));
    connect(propfindJob, &PropfindJob::result, this, &InvalidFilenameDialog::onRemoteSourceFileAlreadyExists);
    connect(propfindJob, &PropfindJob::finishedWithError, this, &InvalidFilenameDialog::onRemoteSourceFileDoesNotExist);
    propfindJob->start();
}

void InvalidFilenameDialog::onRemoteSourceFileAlreadyExists(const QVariantMap &values)
{
    Q_UNUSED(values);

    // Remote source file exists. We need to start MoveJob to rename it
    const auto remoteSource = QDir::cleanPath(_folder->remotePath() + _originalFileName);
    const auto remoteDestionation = QDir::cleanPath(_account->davUrl().path() + _folder->remotePath() + _newFilename);
    const auto moveJob = new MoveJob(_account, remoteSource, remoteDestionation, this);
    connect(moveJob, &MoveJob::finishedSignal, this, &InvalidFilenameDialog::onMoveJobFinished);
    moveJob->start();
}

void InvalidFilenameDialog::onRemoteSourceFileDoesNotExist(QNetworkReply *reply)
{
    Q_UNUSED(reply);

    // It's a new file we've just created locally. We will attempt to rename it locally.
    const auto localSource = QDir::cleanPath(_folder->path() + _originalFileName);
    const auto localDestionation = QDir::cleanPath(_folder->path()+ _newFilename);

    QString error;
    if (!FileSystem::rename(localSource, localDestionation, &error)) {
        _ui->errorLabel->setText(tr("Could not rename local file. %1").arg(error));
        return;
    }
    QDialog::accept();
}
}
