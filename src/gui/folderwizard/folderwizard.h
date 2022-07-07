/*
 * Copyright (C) by Duncan Mac-Vicar P. <duncan@kde.org>
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

#include <QNetworkReply>
#include <QTimer>
#include <QWizard>

#include "accountfwd.h"

#include "gui/folder.h"

class QCheckBox;
class QTreeWidgetItem;

class Ui_FolderWizardTargetPage;

namespace OCC {


class FolderWizardLocalPath;
class FolderWizardRemotePath;
class FolderWizardSelectiveSync;

/**
 * @brief The FolderWizard class
 * @ingroup gui
 */
class FolderWizard : public QWizard
{
    Q_OBJECT
public:
    enum {
        Page_Space,
        Page_Source,
        Page_Target,
        Page_SelectiveSync
    };

    explicit FolderWizard(AccountPtr account, QWidget *parent = nullptr, Qt::WindowFlags flags = {});
    ~FolderWizard() override;

    /***
     * The webdav url for the sync connection.
     */
    QUrl davUrl() const;

    /***
     * The local folder used for the sync.
     */
    QString destination() const;

    /***
     * The Space name to display in the list of folders or an empty string.
     */
    QString displayName() const;

    /***
     * Wether to use virtual files.
     */
    bool useVirtualFiles() const;

    bool eventFilter(QObject *watched, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    AccountPtr _account;
    class SpacesPage *_spacesPage;
    FolderWizardLocalPath *_folderWizardSourcePage = nullptr;
    FolderWizardRemotePath *_folderWizardTargetPage = nullptr;
    FolderWizardSelectiveSync *_folderWizardSelectiveSyncPage = nullptr;
};


} // namespace OCC
