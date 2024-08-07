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

#include <QUrl>
#include <QWizard>

#include "accountfwd.h"
#include "gui/folderman.h"

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
    Q_ENUM(PageType)

    explicit FolderWizard(const AccountStatePtr &account, QWidget *parent = nullptr);
    ~FolderWizard() override;

    FolderMan::SyncConnectionDescription result();

    Q_DECLARE_PRIVATE(FolderWizard)

private:
    QScopedPointer<FolderWizardPrivate> d_ptr;
};

} // namespace OCC
