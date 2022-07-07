/*
 * Copyright (C) by Hannah von Reth <hannah.vonreth@owncloud.com>
 * Copyright (C) by Duncan Mac-Vicar P. <duncan@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
#pragma once

#include "gui/folder.h"

#include <QWizardPage>

class Ui_FolderWizardSourcePage;
namespace OCC {


/**
 * @brief Page to ask for the local source folder
 * @ingroup gui
 */
class FolderWizardLocalPath : public QWizardPage
{
    Q_OBJECT
public:
    explicit FolderWizardLocalPath(const AccountPtr &account, QWidget *parent = nullptr);
    ~FolderWizardLocalPath() override;

    bool isComplete() const override;
    void initializePage() override;
    void cleanupPage() override;
protected slots:
    void slotChooseLocalFolder();

private:
    Ui_FolderWizardSourcePage *_ui;
    QMap<QString, Folder *> _folderMap;
    AccountPtr _account;
};

}
