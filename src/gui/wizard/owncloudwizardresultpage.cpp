/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 * Copyright (C) by Krzesimir Nowak <krzesimir@endocode.com>
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

#include <QDesktopServices>
#include <QDir>
#include <QUrl>

#include "wizard/owncloudwizardresultpage.h"
#include "wizard/owncloudwizardcommon.h"
#include "theme.h"

namespace OCC {

OwncloudWizardResultPage::OwncloudWizardResultPage()
    : QWizardPage()
    , _localFolder()
    , _remoteFolder()
    , _complete(false)
    , _ui()
{
    _ui.setupUi(this);
    // no fields to register.

    setTitle(WizardCommon::subTitleTemplate().arg(tr("Everything set up!")));
    // required to show header in QWizard's modern style
    setSubTitle(QLatin1String(" "));

    _ui.pbOpenLocal->setText(tr("Open Local Folder"));
    _ui.pbOpenLocal->setIcon(QIcon(QLatin1String(":/client/resources/folder-sync.png")));
    _ui.pbOpenLocal->setIconSize(QSize(48, 48));
    _ui.pbOpenLocal->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    connect(_ui.pbOpenLocal, &QAbstractButton::clicked, this, &OwncloudWizardResultPage::slotOpenLocal);

    Theme *theme = Theme::instance();
    QIcon appIcon = theme->applicationIcon();
    _ui.pbOpenServer->setText(tr("Open %1 in Browser").arg(theme->appNameGUI()));
    _ui.pbOpenServer->setIcon(appIcon.pixmap(48));
    _ui.pbOpenServer->setIconSize(QSize(48, 48));
    _ui.pbOpenServer->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    connect(_ui.pbOpenServer, &QAbstractButton::clicked, this, &OwncloudWizardResultPage::slotOpenServer);
    setupCustomization();
}

OwncloudWizardResultPage::~OwncloudWizardResultPage()
{
}

void OwncloudWizardResultPage::setComplete(bool complete)
{
    _complete = complete;
    emit completeChanged();
}

bool OwncloudWizardResultPage::isComplete() const
{
    return _complete;
}

void OwncloudWizardResultPage::initializePage()
{
    _ui.localFolderLabel->setText(QString::null);
}

void OwncloudWizardResultPage::setRemoteFolder(const QString &remoteFolder)
{
    _remoteFolder = remoteFolder;
}

void OwncloudWizardResultPage::setupCustomization()
{
    // set defaults for the customize labels.
    _ui.topLabel->setText(QString::null);
    _ui.topLabel->hide();

    QVariant variant = Theme::instance()->customMedia(Theme::oCSetupResultTop);
    WizardCommon::setupCustomMedia(variant, _ui.topLabel);
}

void OwncloudWizardResultPage::slotOpenLocal()
{
    const QString localFolder = wizard()->property("localFolder").toString();
    QDesktopServices::openUrl(QUrl::fromLocalFile(localFolder));
}

void OwncloudWizardResultPage::slotOpenServer()
{
    Theme *theme = Theme::instance();
    QUrl url = QUrl(field("OCUrl").toString() + theme->wizardUrlPostfix());
    QDesktopServices::openUrl(url);
}

} // namespace OCC
