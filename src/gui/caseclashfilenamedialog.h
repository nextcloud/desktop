/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
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
