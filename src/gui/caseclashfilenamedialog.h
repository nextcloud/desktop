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

#pragma once

#include "accountfwd.h"
#include "caseclashconflictsolver.h"

#include <QDialog>
#include <QLabel>
#include <QToolButton>
#include <QNetworkReply>

#include <memory>

namespace OCC {

class Folder;

namespace Ui {
    class CaseClashFilenameDialog;
}


class CaseClashFilenameDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CaseClashFilenameDialog(AccountPtr account,
                                     Folder *folder,
                                     const QString &conflictFilePath,
                                     const QString &conflictTaggedPath,
                                     QWidget *parent = nullptr);

    ~CaseClashFilenameDialog() override;

    void accept() override;

signals:
    void successfulRename(const QString &filePath);

private slots:
    void updateFileWidgetGroup(const QString &filePath,
                               const QString &linkText,
                               QLabel *filenameLabel,
                               QLabel *linkLabel,
                               QLabel *mtimeLabel,
                               QLabel *sizeLabel,
                               QToolButton *button) const;

private:
    // Find the conflicting file path
    static QString caseClashConflictFile(const QString &conflictFilePath);

    void onFilenameLineEditTextChanged(const QString &text);
    void checkIfAllowedToRename();
    bool processLeadingOrTrailingSpacesError(const QString &fileName);

    std::unique_ptr<Ui::CaseClashFilenameDialog> _ui;
    CaseClashConflictSolver _conflictSolver;
    AccountPtr _account;
    Folder *_folder = nullptr;

    QString _filePath;
    QString _relativeFilePath;
    QString _originalFileName;
    QString _newFilename;
};
}
