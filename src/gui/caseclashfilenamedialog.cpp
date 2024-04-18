/*
 * Copyright 2021 (c) Felix Weilbach <felix.weilbach@nextcloud.com>
 * Copyright 2022 (c) Matthieu Gallien <matthieu.gallien@nextcloud.com>
 * Copyright 2022 (c) Claudio Cambra <claudio.cambra@nextcloud.com>
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

#include "caseclashfilenamedialog.h"
#include "ui_caseclashfilenamedialog.h"

#include "account.h"
#include "folder.h"

#include <QPushButton>
#include <QDir>
#include <QAbstractButton>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QPushButton>
#include <QDirIterator>
#include <QDesktopServices>
#include <QLoggingCategory>

#include <array>

namespace {
constexpr std::array<QChar, 9> caseClashIllegalCharacters({ '\\', '/', ':', '?', '*', '\"', '<', '>', '|' });

QVector<QChar> getCaseClashIllegalCharsFromString(const QString &string)
{
    QVector<QChar> result;
    for (const auto &character : string) {
        if (std::find(caseClashIllegalCharacters.begin(), caseClashIllegalCharacters.end(), character)
            != caseClashIllegalCharacters.end()) {
            result.push_back(character);
        }
    }
    return result;
}

QString caseClashIllegalCharacterListToString(const QVector<QChar> &illegalCharacters)
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

Q_LOGGING_CATEGORY(lcCaseClashConflictFialog, "nextcloud.sync.caseclash.dialog", QtInfoMsg)

CaseClashFilenameDialog::CaseClashFilenameDialog(AccountPtr account,
                                                 Folder *folder,
                                                 const QString &conflictFilePath,
                                                 const QString &conflictTaggedPath,
                                                 QWidget *parent)
    : QDialog(parent)
    , _ui(std::make_unique<Ui::CaseClashFilenameDialog>())
    , _conflictSolver(conflictFilePath, conflictTaggedPath, folder->remotePath(), folder->path(), account, folder->journalDb())
    , _account(account)
    , _folder(folder)
    , _filePath(std::move(conflictFilePath))
{
    Q_ASSERT(_account);
    Q_ASSERT(_folder);

    const auto filePathFileInfo = QFileInfo(_filePath);
    const auto conflictFileName = filePathFileInfo.fileName();

    _relativeFilePath = filePathFileInfo.path() + QStringLiteral("/");
    _relativeFilePath = _relativeFilePath.replace(folder->path(), QLatin1String());
    _relativeFilePath = _relativeFilePath.isEmpty() ? QString() : _relativeFilePath;
    if (!_relativeFilePath.isEmpty() && !_relativeFilePath.endsWith(QStringLiteral("/"))) {
        _relativeFilePath += QStringLiteral("/");
    }

    _originalFileName = _relativeFilePath + conflictFileName;

    _ui->setupUi(this);
    _ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    _ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Rename file"));

    _ui->descriptionLabel->setText(tr("The file \"%1\" could not be synced because of a case clash conflict with an existing file on this system.").arg(_originalFileName));
    _ui->explanationLabel->setText(tr("%1 does not support equal file names with only letter casing differences.").arg(QSysInfo::prettyProductName()));
    _ui->filenameLineEdit->setText(conflictFileName);

    const auto preexistingConflictingFile = caseClashConflictFile(_filePath);
    updateFileWidgetGroup(preexistingConflictingFile,
                          tr("Open existing file"),
                          _ui->localVersionFilename,
                          _ui->localVersionLink,
                          _ui->localVersionMtime,
                          _ui->localVersionSize,
                          _ui->localVersionButton);

    updateFileWidgetGroup(conflictTaggedPath,
                          tr("Open clashing file"),
                          _ui->remoteVersionFilename,
                          _ui->remoteVersionLink,
                          _ui->remoteVersionMtime,
                          _ui->remoteVersionSize,
                          _ui->remoteVersionButton);
    // Display incoming conflict filename, not conflict-tagged filename
    _ui->remoteVersionFilename->setText(filePathFileInfo.fileName());

    adjustSize();

    connect(_ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(_ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(_ui->localVersionButton, &QToolButton::clicked, this, [preexistingConflictingFile] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(preexistingConflictingFile));
    });
    connect(_ui->remoteVersionButton, &QToolButton::clicked, this, [conflictTaggedPath] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(conflictTaggedPath));
    });

    _ui->errorLabel->setText({}/*
        tr("Checking rename permissions …")*/);
    _ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    _ui->filenameLineEdit->setEnabled(false);

    connect(_ui->filenameLineEdit, &QLineEdit::textChanged, this,
        &CaseClashFilenameDialog::onFilenameLineEditTextChanged);

    connect(&_conflictSolver, &CaseClashConflictSolver::errorStringChanged, this, [this] () {
        _ui->errorLabel->setText(_conflictSolver.errorString());
    });

    connect(&_conflictSolver, &CaseClashConflictSolver::allowedToRenameChanged, this, [this] () {
        _ui->buttonBox->setStandardButtons(_ui->buttonBox->standardButtons() &~ QDialogButtonBox::No);
        if (_conflictSolver.allowedToRename()) {
            _ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
            _ui->filenameLineEdit->setEnabled(true);
            _ui->filenameLineEdit->selectAll();
        } else {
            _ui->buttonBox->setStandardButtons(_ui->buttonBox->standardButtons() | QDialogButtonBox::No);
        }
    });

    connect(&_conflictSolver, &CaseClashConflictSolver::failed, this, [this] () {
        _ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    });

    connect(&_conflictSolver, &CaseClashConflictSolver::done, this, [this] () {
        Q_EMIT successfulRename(_folder->remotePath() + _newFilename);
        QDialog::accept();
    });

    checkIfAllowedToRename();
}

CaseClashFilenameDialog::~CaseClashFilenameDialog() = default;

QString CaseClashFilenameDialog::caseClashConflictFile(const QString &conflictFilePath)
{
    const auto filePathFileInfo = QFileInfo(conflictFilePath);
    const auto conflictFileName = filePathFileInfo.fileName();

    QDirIterator it(filePathFileInfo.path(), QDirIterator::Subdirectories);

    while(it.hasNext()) {
        const auto filePath = it.next();
        qCDebug(lcCaseClashConflictFialog) << filePath;
        QFileInfo fileInfo(filePath);

        if(fileInfo.isDir()) {
            continue;
        }

        const auto currentFileName = fileInfo.fileName();
        if (currentFileName.compare(conflictFileName, Qt::CaseInsensitive) == 0 &&
                currentFileName != conflictFileName) {

            return filePath;
        }
    }

    return {};
}

void CaseClashFilenameDialog::updateFileWidgetGroup(const QString &filePath,
                                                    const QString &linkText,
                                                    QLabel *filenameLabel,
                                                    QLabel *linkLabel,
                                                    QLabel *mtimeLabel,
                                                    QLabel *sizeLabel,
                                                    QToolButton *button) const
{
    const auto filePathFileInfo = QFileInfo(filePath);
    const auto filename = filePathFileInfo.fileName();
    const auto lastModifiedString = filePathFileInfo.lastModified().toString();
    const auto fileSizeString = locale().formattedDataSize(filePathFileInfo.size());
    const auto fileUrl = QUrl::fromLocalFile(filePath).toString();
    const auto linkString = QStringLiteral("<a href='%1'>%2</a>").arg(fileUrl, linkText);
    const auto mime = QMimeDatabase().mimeTypeForFile(_filePath, QMimeDatabase::MatchExtension);
    QIcon fileTypeIcon;

    qCDebug(lcCaseClashConflictFialog) << filePath << filePathFileInfo.exists() << filename << lastModifiedString << fileSizeString << fileUrl << linkString << mime;

    if (QIcon::hasThemeIcon(mime.iconName())) {
        fileTypeIcon = QIcon::fromTheme(mime.iconName());
    } else {
        fileTypeIcon = QIcon(":/qt-project.org/styles/commonstyle/images/file-128.png");
    }

    filenameLabel->setText(filename);
    mtimeLabel->setText(lastModifiedString);
    sizeLabel->setText(fileSizeString);
    linkLabel->setText(linkString);
    button->setIcon(fileTypeIcon);
}

void CaseClashFilenameDialog::checkIfAllowedToRename()
{
    _conflictSolver.checkIfAllowedToRename();
}

bool CaseClashFilenameDialog::processLeadingOrTrailingSpacesError(const QString &fileName)
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
        }

        return true;
    }

    return false;
}

void CaseClashFilenameDialog::accept()
{
    _newFilename = _relativeFilePath + _ui->filenameLineEdit->text().trimmed();
    _conflictSolver.solveConflict(_newFilename);
}

void CaseClashFilenameDialog::onFilenameLineEditTextChanged(const QString &text)
{
    const auto isNewFileNameDifferent = text != _originalFileName;
    const auto illegalContainedCharacters = getCaseClashIllegalCharsFromString(text);
    const auto containsIllegalChars = !illegalContainedCharacters.empty() || text.endsWith(QLatin1Char('.'));
    const auto isTextValid = isNewFileNameDifferent && !containsIllegalChars;

    _ui->errorLabel->setText("");

    if (!processLeadingOrTrailingSpacesError(text) && !isTextValid){
        _ui->errorLabel->setText(tr("Filename contains illegal characters: %1").arg(caseClashIllegalCharacterListToString(illegalContainedCharacters)));
    }

    _ui->buttonBox->button(QDialogButtonBox::Ok)
        ->setEnabled(isTextValid);
}
}
