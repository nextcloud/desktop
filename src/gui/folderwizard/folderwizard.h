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

class FolderWizardPrivate;

/**
 * @brief The FolderWizard class
 * @ingroup gui
 */
class FolderWizard : public QWizard
{
    Q_OBJECT
public:
    enum PageType {
        Page_Space,
        Page_Source,
        Page_Target,
        Page_SelectiveSync
    };
    Q_ENUM(PageType);

    struct Result
    {
        /***
-         * The webdav url for the sync connection.
         */
        QUrl davUrl;

        /***
         * The id of the space or empty in case of ownCloud 10.
         */
        QString spaceId;

        /***
         * The local folder used for the sync.
         */
        QString localPath;

        /***
         * The relative remote path
         */
        QString remotePath;

        /***
         * The Space name to display in the list of folders or an empty string.
         */
        QString displayName;

        /***
         * Wether to use virtual files.
         */
        bool useVirtualFiles;

        uint32_t priority;

        QSet<QString> selectiveSyncBlackList;
    };

    explicit FolderWizard(const AccountStatePtr &account, QWidget *parent = nullptr);
    ~FolderWizard() override;

    Result result();

    bool eventFilter(QObject *watched, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

    Q_DECLARE_PRIVATE(FolderWizard);

private:
    QScopedPointer<FolderWizardPrivate> d_ptr;
};

} // namespace OCC
