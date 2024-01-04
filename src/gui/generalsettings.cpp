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

#include "accountmanager.h"
#include "application.h"
#include "common/version.h"
#include "configfile.h"
#include "theme.h"

#include "gui/settingsdialog.h"

#ifdef WITH_AUTO_UPDATER
#include "updater/updater.h"
#include "updater/ocupdater.h"
#ifdef Q_OS_MAC
// FIXME We should unify those, but Sparkle does everything behind the scene transparently
#include "updater/sparkleupdater.h"
#endif
#endif

#include "ignorelisteditor.h"

#include "translations.h"

#include <QDir>
#include <QMessageBox>
#include <QNetworkProxy>
#include <QOperatingSystemVersion>
#include <QScopedValueRollback>

namespace OCC {

GeneralSettings::GeneralSettings(QWidget *parent)
    : QWidget(parent)
    , _ui(new Ui::GeneralSettings)
    , _currentlyLoading(false)
{
    _ui->setupUi(this);

    connect(_ui->desktopNotificationsCheckBox, &QAbstractButton::toggled, this, &GeneralSettings::slotToggleOptionalDesktopNotifications);

    reloadConfig();
    loadMiscSettings();

    // misc
    connect(_ui->monoIconsCheckBox, &QAbstractButton::toggled, this, &GeneralSettings::saveMiscSettings);
    connect(_ui->crashreporterCheckBox, &QAbstractButton::toggled, this, &GeneralSettings::saveMiscSettings);
    connect(_ui->newFolderLimitCheckBox, &QAbstractButton::toggled, this, &GeneralSettings::saveMiscSettings);
    connect(_ui->newFolderLimitSpinBox, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &GeneralSettings::saveMiscSettings);
    connect(_ui->newExternalStorage, &QAbstractButton::toggled, this, &GeneralSettings::saveMiscSettings);

    connect(_ui->languageDropdown, QOverload<int>::of(&QComboBox::activated), this, [this]() {
        // first, store selected language in config file
        saveMiscSettings();

        // warn user that a language change requires a restart to take effect
        QMessageBox::warning(this, tr("Warning"), tr("Language changes require a restart of this application to take effect."), QMessageBox::Ok);
    });

    /* handle the hidden file checkbox */

    /* the ignoreHiddenFiles flag is a folder specific setting, but for now, it is
     * handled globally. Save it to every folder that is defined.
     */
    connect(_ui->syncHiddenFilesCheckBox, &QCheckBox::toggled, this, [](bool checked) { FolderMan::instance()->setIgnoreHiddenFiles(!checked); });

    _ui->crashreporterCheckBox->setVisible(Theme::instance()->withCrashReporter());

    _ui->moveToTrashCheckBox->setVisible(Theme::instance()->enableMoveToTrash());
    connect(_ui->moveToTrashCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        ConfigFile().setMoveToTrash(checked);
        Q_EMIT syncOptionsChanged();
    });

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
    connect(_ui->logSettingsButton, &QPushButton::clicked, this, [] {
        // only access occApp after things are set up
        ocApp()->gui()->slotToggleLogBrowser();
    });

    // accountAdded means the wizard was finished and the wizard might change some options.
    connect(AccountManager::instance(), &AccountManager::accountAdded, this, &GeneralSettings::loadMiscSettings);

    // Only our standard brandings currently support beta channel
#ifdef WITH_AUTO_UPDATER
    if (Theme::instance()->appName() != QLatin1String("testpilotcloud")) {
#ifdef Q_OS_MAC
        // Because we don't have any statusString from the SparkleUpdater anyway we can hide the whole thing
        _ui->updaterWidget->hide();
#else
        _ui->updateChannelLabel->hide();
        _ui->updateChannel->hide();
        if (ConfigFile().updateChannel() != QLatin1String("stable")) {
            ConfigFile().setUpdateChannel(QStringLiteral("stable"));
        }
#endif
    }
    // we want to attach the known english identifiers which are also used within the configuration file as user data inside the data model
    // that way, when we intend to reset to the original selection when the dialog, we can look up the config file's stored value in the data model
    _ui->updateChannel->addItem(tr("stable"), QStringLiteral("stable"));
    _ui->updateChannel->addItem(tr("beta"), QStringLiteral("beta"));

    if (!ConfigFile().skipUpdateCheck() && Updater::instance()) {
        // Channel selection
        _ui->updateChannel->setCurrentIndex(_ui->updateChannel->findData(ConfigFile().updateChannel()));
        connect(_ui->updateChannel, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &GeneralSettings::slotUpdateChannelChanged);

        // Note: the sparkle-updater is not an OCUpdater
        if (auto *ocupdater = qobject_cast<OCUpdater *>(Updater::instance())) {
            auto updateInfo = [ocupdater, this] {
                _ui->updateStateLabel->setText(ocupdater->statusString());
                _ui->restartButton->setVisible(ocupdater->downloadState() == OCUpdater::DownloadComplete);
            };
            connect(ocupdater, &OCUpdater::downloadStateChanged, this, updateInfo);
            connect(_ui->restartButton, &QAbstractButton::clicked, ocupdater, &OCUpdater::slotStartInstaller);
            connect(_ui->restartButton, &QAbstractButton::clicked, qApp, &QApplication::quit);
            updateInfo();
        }
#ifdef HAVE_SPARKLE
        if (SparkleUpdater *sparkleUpdater = qobject_cast<SparkleUpdater *>(Updater::instance())) {
            _ui->updateStateLabel->setText(sparkleUpdater->statusString());
            _ui->restartButton->setVisible(false);
        }
#endif
    } else {
        _ui->updaterWidget->hide();
    }
#else
    _ui->updaterWidget->hide();
#endif
    connect(_ui->about_pushButton, &QPushButton::clicked, this, &GeneralSettings::showAbout);

    if (!Theme::instance()->aboutShowCopyright()) {
        _ui->copyrightLabel->hide();
    }
    if (Theme::instance()->forceVirtualFilesOption() && VfsPluginManager::instance().bestAvailableVfsMode() == Vfs::WindowsCfApi) {
        _ui->groupBox_non_vfs->hide();
    }
}

GeneralSettings::~GeneralSettings()
{
    delete _ui;
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

    // the dropdown has to be populated before we can can pick an entry below based on the stored setting
    loadLanguageNamesIntoDropdown();

    const auto &locale = cfgFile.uiLanguage();
    const auto index = _ui->languageDropdown->findData(locale);
    _ui->languageDropdown->setCurrentIndex(index < 0 ? 0 : index);
}

void GeneralSettings::showEvent(QShowEvent *)
{
    reloadConfig();
}

void GeneralSettings::slotUpdateChannelChanged([[maybe_unused]] int index)
{
#ifdef WITH_AUTO_UPDATER
    QString channel;
    if (index < 0) {
        // invalid index reset to stable
        channel = QStringLiteral("stable");
    } else {
        channel = _ui->updateChannel->itemData(index).toString();
    }
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
    auto acceptButton = msgBox->addButton(tr("Change update channel"), QMessageBox::AcceptRole);
    msgBox->addButton(tr("Cancel"), QMessageBox::RejectRole);
    connect(msgBox, &QMessageBox::finished, msgBox, [this, channel, msgBox, acceptButton, index] {
        msgBox->deleteLater();
        if (msgBox->clickedButton() == acceptButton) {
            ConfigFile().setUpdateChannel(channel);
            if (OCUpdater *updater = qobject_cast<OCUpdater *>(Updater::instance())) {
                updater->setUpdateUrl(Updater::updateUrl());
                updater->checkForUpdate();
            }
#if defined(Q_OS_MAC) && defined(HAVE_SPARKLE)
            else if (SparkleUpdater *updater = qobject_cast<SparkleUpdater *>(Updater::instance())) {
                updater->setUpdateUrl(Updater::updateUrl());
                updater->checkForUpdate();
            }
#endif
        } else {
            const auto oldChannel = _ui->updateChannel->findData(ConfigFile().updateChannel());
            Q_ASSERT(oldChannel >= 0);
            Q_ASSERT(oldChannel <= 1);
            _ui->updateChannel->setCurrentIndex(oldChannel);
        }
    });
    msgBox->open();
#endif
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

    // the first entry, identified by index 0, means "use default", which is a special case handled below
    const QString pickedLocale = _ui->languageDropdown->currentData().toString();
    cfgFile.setUiLanguage(pickedLocale);
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
        _ignoreEditor = new IgnoreListEditor(ocApp()->gui()->settingsDialog());
        _ignoreEditor->setAttribute(Qt::WA_DeleteOnClose, true);
    }
    ownCloudGui::raiseDialog(_ignoreEditor);
}

void GeneralSettings::reloadConfig()
{
    _ui->syncHiddenFilesCheckBox->setChecked(!FolderMan::instance()->ignoreHiddenFiles());
    _ui->moveToTrashCheckBox->setChecked(ConfigFile().moveToTrash());
    if (Utility::hasSystemLaunchOnStartup(Theme::instance()->appName())) {
        _ui->autostartCheckBox->setChecked(true);
        _ui->autostartCheckBox->setDisabled(true);
        _ui->autostartCheckBox->setToolTip(tr("You cannot disable autostart because system-wide autostart is enabled."));
    } else {
        const bool hasAutoStart = Utility::hasLaunchOnStartup(Theme::instance()->appName());
        // make sure the binary location is correctly set
        slotToggleLaunchOnStartup(hasAutoStart);
        _ui->autostartCheckBox->setChecked(hasAutoStart);
        connect(_ui->autostartCheckBox, &QAbstractButton::toggled, this, &GeneralSettings::slotToggleLaunchOnStartup);
    }
}

void GeneralSettings::loadLanguageNamesIntoDropdown()
{
    // allow method to be called more than once
    _ui->languageDropdown->clear();

    // if no option has been chosen explicitly by the user, the first entry shall be used
    _ui->languageDropdown->addItem(tr("(use default)"));

    // initialize map of locales to language names
    const auto availableLocales = []() {
        auto rv = Translations::listAvailableTranslations().values();
        rv.sort(Qt::CaseInsensitive);
        return rv;
    }();

    for (const auto &availableLocale : availableLocales) {
        auto nativeLanguageName = QLocale(availableLocale).nativeLanguageName();

        // fallback if there's a locale whose name Qt doesn't know
        // this indicates a broken filename
        if (nativeLanguageName.isEmpty()) {
            qCDebug(lcApplication()) << "Warning: could not find native language name for locale" << availableLocale;
            nativeLanguageName = tr("unknown (%1)").arg(availableLocale);
        }

        QString entryText = QStringLiteral("%1 (%2)").arg(nativeLanguageName, availableLocale);
        _ui->languageDropdown->addItem(entryText, availableLocale);
    }
}

} // namespace OCC
