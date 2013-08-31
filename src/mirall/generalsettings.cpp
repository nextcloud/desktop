/*
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include "generalsettings.h"
#include "ui_generalsettings.h"

#include "mirall/theme.h"
#include "mirall/mirallconfigfile.h"
#include "mirall/application.h"
#include "mirall/utility.h"
#include "mirall/mirallconfigfile.h"

#include <QNetworkProxy>
#include <QDir>

namespace Mirall {

GeneralSettings::GeneralSettings(QWidget *parent) :
    QWidget(parent),
    _ui(new Ui::GeneralSettings)
{
    _ui->setupUi(this);

    connect(_ui->desktopNotificationsCheckBox, SIGNAL(toggled(bool)),
            SLOT(slotToggleOptionalDesktopNotifications(bool)));

    _ui->autostartCheckBox->setChecked(Utility::hasLaunchOnStartup(Theme::instance()->appName()));
    connect(_ui->autostartCheckBox, SIGNAL(toggled(bool)), SLOT(slotToggleLaunchOnStartup(bool)));

    // setup about section
    QString about = Theme::instance()->about();
    if (about.isEmpty()) {
        _ui->aboutGroupBox->hide();
    } else {
        _ui->aboutLabel->setText(about);
        _ui->aboutLabel->setOpenExternalLinks(true);
    }

    loadMiscSettings();

    // misc
    connect(_ui->monoIconsCheckBox, SIGNAL(toggled(bool)), SLOT(saveMiscSettings()));

    // OEM themes are not obliged to ship mono icons, so there
    // is no point in offering an option
    QString themeDir = QString::fromLatin1(":/mirall/theme/%1/")
            .arg(Theme::instance()->systrayIconFlavor(true));
    _ui->monoIconsCheckBox->setVisible(QDir(themeDir).exists());

}

GeneralSettings::~GeneralSettings()
{
    delete _ui;
}

void GeneralSettings::loadMiscSettings()
{
    MirallConfigFile cfgFile;
    _ui->monoIconsCheckBox->setChecked(cfgFile.monoIcons());
    _ui->desktopNotificationsCheckBox->setChecked(cfgFile.optionalDesktopNotifications());
}

void GeneralSettings::saveMiscSettings()
{
    MirallConfigFile cfgFile;
    bool isChecked = _ui->monoIconsCheckBox->isChecked();
    cfgFile.setMonoIcons(isChecked);
    Theme::instance()->setSystrayUseMonoIcons(isChecked);
}

void GeneralSettings::slotToggleLaunchOnStartup(bool enable)
{
    Theme *theme = Theme::instance();
    Utility::setLaunchOnStartup(theme->appName(), theme->appNameGUI(), enable);
}

void GeneralSettings::slotToggleOptionalDesktopNotifications(bool enable)
{
    MirallConfigFile cfgFile;
    cfgFile.setOptionalDesktopNotifications(enable);
}

} // namespace Mirall
