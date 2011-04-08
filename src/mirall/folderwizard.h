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

#ifndef MIRALL_FOLDERWIZARD_H
#define MIRALL_FOLDERWIZARD_H

#include <QWizard>

#include "ui_folderwizardsourcepage.h"
#include "ui_folderwizardtargetpage.h"

namespace Mirall {

/**
 * page to ask for the local source folder
 */
class FolderWizardSourcePage : public QWizardPage
{
    Q_OBJECT
public:
    FolderWizardSourcePage();
    ~FolderWizardSourcePage();

    virtual bool isComplete() const;

protected slots:
    void on_localFolderChooseBtn_clicked();
    void on_localFolderLineEdit_textChanged();

private:
    Ui_FolderWizardSourcePage _ui;
};


/**
 * page to ask for the target folder
 */

class FolderWizardTargetPage : public QWizardPage
{
    Q_OBJECT
public:
    FolderWizardTargetPage();
    ~FolderWizardTargetPage();

    virtual bool isComplete() const;

    virtual void initializePage();
protected slots:
    void slotToggleItems();
    void on_localFolder2ChooseBtn_clicked();

    void on_localFolderRadioBtn_toggled();
    void on_urlFolderRadioBtn_toggled();
    void on_checkBoxOnlyOnline_toggled();

    void on_localFolder2LineEdit_textChanged();
    void on_urlFolderLineEdit_textChanged();

private:
    Ui_FolderWizardTargetPage _ui;
};


/**
 * Available fields registered:
 *
 * alias
 * sourceFolder
 * local?
 * remote?
 * targetLocalFolder
 * targetURLFolder
 *
 */
class FolderWizard : public QWizard
{
    Q_OBJECT
public:

    enum {
        Page_Source,
        Page_Target
    };

    FolderWizard(QWidget *parent = 0L);

};


} // ns Mirall

#endif
