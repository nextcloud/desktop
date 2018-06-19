/*
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
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

#include "generalsettings.h"
#include "ui_generalsettings.h"

#include "theme.h"
#include "configfile.h"
#include "application.h"
#include "configfile.h"
#include "owncloudsetupwizard.h"
#include "accountmanager.h"
#include "synclogdialog.h"

#include "updater/updater.h"
#include "updater/ocupdater.h"
#ifdef Q_OS_MAC
// FIXME We should unify those, but Sparkle does everything behind the scene transparently
#include "updater/sparkleupdater.h"
#endif
#include "ignorelisteditor.h"

#include "config.h"

#include <QNetworkProxy>
#include <QDir>
#include <QScopedValueRollback>
#include <QMessageBox>

namespace OCC {

GeneralSettings::GeneralSettings(QWidget *parent)
    : QWidget(parent)
    , _ui(new Ui::GeneralSettings)
    , _currentlyLoading(false)
{
    _ui->setupUi(this);

    connect(_ui->desktopNotificationsCheckBox, &QAbstractButton::toggled,
        this, &GeneralSettings::slotToggleOptionalDesktopNotifications);

    _ui->autostartCheckBox->setChecked(Utility::hasLaunchOnStartup(Theme::instance()->appName()));
    connect(_ui->autostartCheckBox, &QAbstractButton::toggled, this, &GeneralSettings::slotToggleLaunchOnStartup);

    // setup about section
    QString about = Theme::instance()->about();
    if (about.isEmpty()) {
        _ui->aboutGroupBox->hide();
    } else {
        _ui->aboutLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextBrowserInteraction);
        _ui->aboutLabel->setText(about);
        _ui->aboutLabel->setWordWrap(true);
        _ui->aboutLabel->setOpenExternalLinks(true);
    }

    loadMiscSettings();
    slotUpdateInfo();

    // misc
    connect(_ui->monoIconsCheckBox, &QAbstractButton::toggled, this, &GeneralSettings::saveMiscSettings);
    connect(_ui->crashreporterCheckBox, &QAbstractButton::toggled, this, &GeneralSettings::saveMiscSettings);
    connect(_ui->newFolderLimitCheckBox, &QAbstractButton::toggled, this, &GeneralSettings::saveMiscSettings);
    connect(_ui->newFolderLimitSpinBox, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &GeneralSettings::saveMiscSettings);
    connect(_ui->newExternalStorage, &QAbstractButton::toggled, this, &GeneralSettings::saveMiscSettings);

#ifndef WITH_CRASHREPORTER
    _ui->crashreporterCheckBox->setVisible(false);
#endif

    /* Set the left contents margin of the layout to zero to make the checkboxes
     * align properly vertically , fixes bug #3758
     */
    int m0, m1, m2, m3;
    _ui->horizontalLayout_3->getContentsMargins(&m0, &m1, &m2, &m3);
    _ui->horizontalLayout_3->setContentsMargins(0, m1, m2, m3);

    // OEM themes are not obliged to ship mono icons, so there
    // is no point in offering an option
    _ui->monoIconsCheckBox->setVisible(Theme::instance()->monoIconsAvailable());

    connect(_ui->ignoredFilesButton, &QAbstractButton::clicked, this, &GeneralSettings::slotIgnoreFilesEditor);

    // accountAdded means the wizard was finished and the wizard might change some options.
    connect(AccountManager::instance(), &AccountManager::accountAdded, this, &GeneralSettings::loadMiscSettings);

    // Only our standard brandings currently support beta channel
    Theme *theme = Theme::instance();
    if (theme->appName() != QLatin1String("ownCloud") && theme->appName() != QLatin1String("testpilotcloud") ) {
#ifdef Q_OS_MAC
        // Because we don't have any statusString from the SparkleUpdater anyway we can hide the whole thing
        _ui->updatesGroupBox->hide();
#else
        _ui->updateChannelLabel->hide();
        _ui->updateChannel->hide();
#endif
    }
}

GeneralSettings::~GeneralSettings()
{
    delete _ui;
    delete _syncLogDialog;
}

QSize GeneralSettings::sizeHint() const
{
    return QSize(ownCloudGui::settingsDialogSize().width(), QWidget::sizeHint().height());
}

void GeneralSettings::loadMiscSettings()
{
    QScopedValueRollback<bool> scope(_currentlyLoading, true);
    ConfigFile cfgFile;
    _ui->monoIconsCheckBox->setChecked(cfgFile.monoIcons());
    _ui->desktopNotificationsCheckBox->setChecked(cfgFile.optionalDesktopNotifications());
    _ui->crashreporterCheckBox->setChecked(cfgFile.crashReporter());
    auto newFolderLimit = cfgFile.newBigFolderSizeLimit();
    _ui->newFolderLimitCheckBox->setChecked(newFolderLimit.first);
    _ui->newFolderLimitSpinBox->setValue(newFolderLimit.second);
    _ui->newExternalStorage->setChecked(cfgFile.confirmExternalStorage());
    _ui->monoIconsCheckBox->setChecked(cfgFile.monoIcons());
}

void GeneralSettings::slotUpdateInfo()
{
    if (ConfigFile().skipUpdateCheck() || !Updater::instance()) {
        // updater disabled on compile
        _ui->updatesGroupBox->setVisible(false);
        return;
    }

    // Note: the sparkle-updater is not an OCUpdater
    OCUpdater *ocupdater = qobject_cast<OCUpdater *>(Updater::instance());
    if (ocupdater) {
        connect(ocupdater, &OCUpdater::downloadStateChanged, this, &GeneralSettings::slotUpdateInfo, Qt::UniqueConnection);
        connect(_ui->restartButton, &QAbstractButton::clicked, ocupdater, &OCUpdater::slotStartInstaller, Qt::UniqueConnection);
        connect(_ui->restartButton, &QAbstractButton::clicked, qApp, &QApplication::quit, Qt::UniqueConnection);

        _ui->updateStateLabel->setText(ocupdater->statusString());
        _ui->restartButton->setVisible(ocupdater->downloadState() == OCUpdater::DownloadComplete);

    }
#ifdef Q_OS_MAC
    else if (SparkleUpdater *sparkleUpdater = qobject_cast<SparkleUpdater *>(Updater::instance())) {
        _ui->updateStateLabel->setText(sparkleUpdater->statusString());
        _ui->restartButton->setVisible(false);
    }
#endif

    // Channel selection
    _ui->updateChannel->setCurrentIndex(ConfigFile().updateChannel() == "beta" ? 1 : 0);
    connect(_ui->updateChannel, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, &GeneralSettings::slotUpdateChannelChanged, Qt::UniqueConnection);
}

void GeneralSettings::slotUpdateChannelChanged(int index)
{
    QString channel = index == 0 ? QStringLiteral("stable") : QStringLiteral("beta");
    if (channel == ConfigFile().updateChannel())
        return;

    auto msgBox = new QMessageBox(
        QMessageBox::Warning,
        tr("Change update channel?"),
        tr("The update channel determines which client updates will be offered "
           "for installation. The \"stable\" channel contains only upgrades that "
           "are considered reliable, while the versions in the \"beta\" channel "
           "may contain newer features and bugfixes, but have not yet been tested "
           "thoroughly."
           "\n\n"
           "Note that this selects only what pool upgrades are taken from, and that "
           "there are no downgrades: So going back from the beta channel to "
           "the stable channel usually cannot be done immediately and means waiting "
           "for a stable version that is newer than the currently installed beta "
           "version."),
        QMessageBox::NoButton,
        this);
    msgBox->addButton(tr("Change update channel"), QMessageBox::AcceptRole);
    msgBox->addButton(tr("Cancel"), QMessageBox::RejectRole);
    connect(msgBox, &QMessageBox::finished, msgBox, [this, channel, msgBox](int result) {
        msgBox->deleteLater();
        if (result == QMessageBox::AcceptRole) {
            ConfigFile().setUpdateChannel(channel);
            if (OCUpdater *updater = qobject_cast<OCUpdater *>(Updater::instance())) {
                updater->setUpdateUrl(Updater::updateUrl());
                updater->checkForUpdate();
            }
#ifdef Q_OS_MAC
            else if (SparkleUpdater *updater = qobject_cast<SparkleUpdater *>(Updater::instance())) {
                updater->setUpdateUrl(Updater::updateUrl());
                updater->checkForUpdate();
            }
#endif
        } else {
            _ui->updateChannel->setCurrentText(ConfigFile().updateChannel());
        }
    });
    msgBox->open();
}

void GeneralSettings::saveMiscSettings()
{
    if (_currentlyLoading)
        return;
    ConfigFile cfgFile;
    bool isChecked = _ui->monoIconsCheckBox->isChecked();
    cfgFile.setMonoIcons(isChecked);
    Theme::instance()->setSystrayUseMonoIcons(isChecked);
    cfgFile.setCrashReporter(_ui->crashreporterCheckBox->isChecked());

    cfgFile.setNewBigFolderSizeLimit(_ui->newFolderLimitCheckBox->isChecked(),
        _ui->newFolderLimitSpinBox->value());
    cfgFile.setConfirmExternalStorage(_ui->newExternalStorage->isChecked());
}

void GeneralSettings::slotToggleLaunchOnStartup(bool enable)
{
    Theme *theme = Theme::instance();
    Utility::setLaunchOnStartup(theme->appName(), theme->appNameGUI(), enable);
}

void GeneralSettings::slotToggleOptionalDesktopNotifications(bool enable)
{
    ConfigFile cfgFile;
    cfgFile.setOptionalDesktopNotifications(enable);
}

void GeneralSettings::slotIgnoreFilesEditor()
{
    if (_ignoreEditor.isNull()) {
        _ignoreEditor = new IgnoreListEditor(this);
        _ignoreEditor->setAttribute(Qt::WA_DeleteOnClose, true);
        _ignoreEditor->open();
    } else {
        ownCloudGui::raiseDialog(_ignoreEditor);
    }
}

} // namespace OCC
