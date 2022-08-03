/*
 * Copyright (C) by Hannah von Reth <hannah.vonreth@owncloud.com>
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
#include "aboutdialog.h"
#include "ui_aboutdialog.h"

#include "theme.h"
#include "guiutility.h"

namespace OCC {

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent, Qt::Sheet)
    , ui(new Ui::AboutDialog)
{
    setWindowModality(Qt::ApplicationModal);
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    ui->setupUi(this);
    setWindowTitle(tr("About %1").arg(Theme::instance()->appNameGUI()));
    ui->aboutText->setText(Theme::instance()->about());
    ui->icon->setPixmap(Theme::instance()->aboutIcon().pixmap(256));
    ui->versionInfo->setText(Theme::instance()->aboutVersions(Theme::VersionFormat::RichText));

    connect(ui->versionInfo, &QTextBrowser::anchorClicked, this, &AboutDialog::openBrowserFromUrl);
    connect(ui->aboutText, &QLabel::linkActivated, this, &AboutDialog::openBrowser);
}

AboutDialog::~AboutDialog()
{
    delete ui;
}

void AboutDialog::openBrowser(const QString &s)
{
    Utility::openBrowser(s, this);
}

void AboutDialog::openBrowserFromUrl(const QUrl &s)
{
    return openBrowser(s.toString());
}

}
