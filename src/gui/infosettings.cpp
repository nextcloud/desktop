/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "infosettings.h"
#include "ui_infosettings.h"

#include "configfile.h"
#include "guiutility.h"
#include "legalnotice.h"
#include "owncloudgui.h"
#include "settingspanelstyle.h"
#include "theme.h"

#if defined(BUILD_UPDATER)
#include "updater/updater.h"
#include "updater/ocupdater.h"
#ifdef Q_OS_MACOS
// FIXME We should unify those, but Sparkle does everything behind the scene transparently
#include "updater/sparkleupdater.h"
#endif
#endif

#include <QAbstractButton>
#include <QComboBox>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QUrl>

namespace OCC {

InfoSettings::InfoSettings(QWidget *parent)
    : QWidget(parent)
    , _ui(new Ui::InfoSettings)
{
    _ui->setupUi(this);

    _ui->infoAndUpdatesLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextBrowserInteraction);
    _ui->infoAndUpdatesLabel->setText(Theme::instance()->about());
    _ui->infoAndUpdatesLabel->setOpenExternalLinks(true);

    connect(_ui->legalNoticeButton, &QPushButton::clicked, this, &InfoSettings::slotShowLegalNotice);

    connect(_ui->usageDocumentationButton, &QPushButton::clicked, this, []() {
        Utility::openBrowser(QUrl(Theme::instance()->helpUrl()));
    });

#if defined(BUILD_UPDATER)
    loadUpdateChannelsList();
#endif

    customizeStyle();
}

InfoSettings::~InfoSettings()
{
    delete _ui;
}

QSize InfoSettings::sizeHint() const
{
    return {
        ownCloudGui::settingsDialogSize().width(),
        QWidget::sizeHint().height()
    };
}

#if defined(BUILD_UPDATER)
void InfoSettings::loadUpdateChannelsList() {
    ConfigFile cfgFile;
    if (cfgFile.serverHasValidSubscription()) {
        _ui->updateChannel->hide();
        _ui->updateChannelLabel->hide();
        _ui->restoreUpdateChannelButton->hide();
        return;
    }

    const auto validUpdateChannels = cfgFile.validUpdateChannels();
    const auto currentUpdateChannel = cfgFile.currentUpdateChannel();
    if (_currentUpdateChannelList.isEmpty() || _currentUpdateChannelList != validUpdateChannels){
        _currentUpdateChannelList = validUpdateChannels;
        _ui->updateChannel->clear();
        _ui->updateChannel->addItems(_currentUpdateChannelList);
        const auto currentUpdateChannelIndex = _currentUpdateChannelList.indexOf(currentUpdateChannel);
        _ui->updateChannel->setCurrentIndex(currentUpdateChannelIndex != -1 ? currentUpdateChannelIndex : 0);
        connect(_ui->updateChannel, &QComboBox::currentTextChanged, this, &InfoSettings::slotUpdateChannelChanged);
    }

    const auto defaultUpdateChannel = cfgFile.defaultUpdateChannel();
    _ui->restoreUpdateChannelButton->setText(tr("Restore to &%1").arg(updateChannelToLocalized(defaultUpdateChannel)));
    _ui->restoreUpdateChannelButton->setEnabled(currentUpdateChannel != defaultUpdateChannel);
    connect(_ui->restoreUpdateChannelButton, &QPushButton::clicked, this, &InfoSettings::slotRestoreUpdateChannel);
}

void InfoSettings::slotUpdateInfo()
{
    ConfigFile config;
    const auto updater = Updater::instance();
    if (config.skipUpdateCheck() || !updater) {
        _ui->updatesContainer->setVisible(false);
        _ui->updatesGroupBox->setVisible(false);
        return;
    }

    _ui->updatesGroupBox->setVisible(true);
    _ui->updatesContainer->setVisible(true);

    if (updater) {
        connect(_ui->updateButton,
                &QAbstractButton::clicked,
                this,
                &InfoSettings::slotUpdateCheckNow,
                Qt::UniqueConnection);
        connect(_ui->autoCheckForUpdatesCheckBox, &QAbstractButton::toggled, this,
                &InfoSettings::slotToggleAutoUpdateCheck, Qt::UniqueConnection);
        _ui->autoCheckForUpdatesCheckBox->setChecked(config.autoUpdateCheck());
    }

    const auto ocupdater = qobject_cast<OCUpdater *>(updater);
    if (ocupdater) {
        connect(ocupdater, &OCUpdater::downloadStateChanged, this, &InfoSettings::slotUpdateInfo, Qt::UniqueConnection);
        connect(_ui->restartButton, &QAbstractButton::clicked, ocupdater, &OCUpdater::slotStartInstaller, Qt::UniqueConnection);

        auto status = ocupdater->statusString(OCUpdater::UpdateStatusStringFormat::Html);
        if (config.serverHasValidSubscription()) {
            auto currentChannel = updateChannelToLocalized(config.currentUpdateChannel());
            if (currentChannel.isEmpty()) {
                currentChannel = config.currentUpdateChannel();
            }
            status.append(QStringLiteral("<br/>%1")
                              .arg(tr("Connected to an enterprise system. Update channel (%1) cannot be changed.")
                                       .arg(currentChannel)));
        }
        Theme::replaceLinkColorStringBackgroundAware(status);

        _ui->updateStateLabel->setOpenExternalLinks(false);
        connect(_ui->updateStateLabel, &QLabel::linkActivated, this, [](const QString &link) {
            Utility::openBrowser(QUrl(link));
        });
        _ui->updateStateLabel->setText(status);
        _ui->restartButton->setVisible(ocupdater->downloadState() == OCUpdater::DownloadComplete);
        _ui->updateButton->setEnabled(ocupdater->downloadState() != OCUpdater::CheckingServer &&
                                      ocupdater->downloadState() != OCUpdater::Downloading &&
                                      ocupdater->downloadState() != OCUpdater::DownloadComplete);
    }
#if defined(Q_OS_MACOS) && defined(HAVE_SPARKLE)
    else if (const auto sparkleUpdater = qobject_cast<SparkleUpdater *>(updater)) {
        connect(sparkleUpdater, &SparkleUpdater::statusChanged, this, &InfoSettings::slotUpdateInfo, Qt::UniqueConnection);
        auto status = sparkleUpdater->statusString();
        if (config.serverHasValidSubscription()) {
            const auto currentChannel = config.currentUpdateChannel();
            if (Qt::mightBeRichText(status)) {
                status.append(QStringLiteral("<br/>"));
            } else {
                status.append(QStringLiteral("\n"));
            }
            status.append(tr("Connected to an enterprise system. Update channel (%1) cannot be changed.")
                        .arg(currentChannel));
        }
        _ui->updateStateLabel->setText(status);
        _ui->restartButton->setVisible(false);

        const auto updaterState = sparkleUpdater->state();
        const auto enableUpdateButton = updaterState == SparkleUpdater::State::Idle ||
                                        updaterState == SparkleUpdater::State::Unknown;
        _ui->updateButton->setEnabled(enableUpdateButton);
    }
#endif
}

void InfoSettings::setAndCheckNewUpdateChannel(const QString &newChannel) {
    ConfigFile().setUpdateChannel(newChannel);
    if (auto updater = qobject_cast<OCUpdater *>(Updater::instance())) {
        updater->setUpdateUrl(Updater::updateUrl());
        updater->checkForUpdate();
    }
#if defined(Q_OS_MACOS) && defined(HAVE_SPARKLE)
    else if (auto updater = qobject_cast<SparkleUpdater *>(Updater::instance())) {
        updater->setUpdateUrl(Updater::updateUrl());
        updater->checkForUpdate();
    }
#endif
}

QString InfoSettings::updateChannelToLocalized(const QString &channel) const
{
    if (channel == QStringLiteral("stable")) {
        return tr("stable");
    }

    if (channel == QStringLiteral("beta")) {
        return tr("beta");
    }

    if (channel == QStringLiteral("daily")) {
        return tr("daily");
    }

    if (channel == QStringLiteral("enterprise")) {
        return tr("enterprise");
    }

    return QString{};
}

void InfoSettings::slotUpdateChannelChanged()
{
    const auto updateChannelFromLocalized = [](const int index) {
        switch(index) {
        case 1:
            return QStringLiteral("beta");
        case 2:
            return QStringLiteral("daily");
        case 3:
            return QStringLiteral("enterprise");
        default:
            return QStringLiteral("stable");
        }
    };

    ConfigFile configFile;
    const auto newChannel = updateChannelFromLocalized(_ui->updateChannel->currentIndex());
    const auto currentUpdateChannel = configFile.currentUpdateChannel();
    if (newChannel == currentUpdateChannel) {
        return;
    }

    if (newChannel == configFile.defaultUpdateChannel()) {
        restoreUpdateChannel();
        return;
    }

    _ui->restoreUpdateChannelButton->setEnabled(true);

    const auto nonEnterpriseOptions = tr("- beta: contains versions with new features that may not be tested thoroughly\n"
                                    "- daily: contains versions created daily only for testing and development\n"
                                    "\n"
                                    "Downgrading versions is not possible immediately: changing from beta to stable means waiting for the new stable version.",
                                    "list of available update channels to non enterprise users and downgrading warning");
    const auto enterpriseOptions = tr("- enterprise: contains stable versions for customers.\n"
                                    "\n"
                                    "Downgrading versions is not possible immediately: changing from stable to enterprise means waiting for the new enterprise version.",
                                    "list of available update channels to enterprise users and downgrading warning");

    auto msgBox = new QMessageBox(
        QMessageBox::Warning,
        tr("Changing update channel?"),
        tr("The channel determines which upgrades will be offered to install:\n"
           "- stable: contains tested versions considered reliable\n",
           "starts list of available update channels, stable is always available")
            .append(configFile.validUpdateChannels().contains("enterprise") ? enterpriseOptions : nonEnterpriseOptions),
        QMessageBox::NoButton,
        this);
    const auto acceptButton = msgBox->addButton(tr("Change update channel"), QMessageBox::AcceptRole);
    msgBox->addButton(tr("Cancel"), QMessageBox::RejectRole);
    connect(msgBox, &QMessageBox::finished, msgBox, [this, newChannel, currentUpdateChannel, msgBox, acceptButton] {
        msgBox->deleteLater();
        if (msgBox->clickedButton() == acceptButton) {
            setAndCheckNewUpdateChannel(newChannel);
        } else {
            _ui->updateChannel->setCurrentText(updateChannelToLocalized(currentUpdateChannel));
        }
    });
    msgBox->open();
}

void InfoSettings::slotUpdateCheckNow()
{
#if defined(Q_OS_MACOS) && defined(HAVE_SPARKLE)
    auto *updater = qobject_cast<SparkleUpdater *>(Updater::instance());
#else
    auto *updater = qobject_cast<OCUpdater *>(Updater::instance());
#endif
    if (ConfigFile().skipUpdateCheck()) {
        updater = nullptr;
    }

    if (updater) {
        _ui->updateButton->setEnabled(false);
        updater->checkForUpdate();
    }
}

void InfoSettings::slotToggleAutoUpdateCheck()
{
    ConfigFile().setAutoUpdateCheck(_ui->autoCheckForUpdatesCheckBox->isChecked(), QString());
}

void InfoSettings::restoreUpdateChannel()
{
    const auto defaultUpdateChannel = ConfigFile().defaultUpdateChannel();
    _ui->restoreUpdateChannelButton->setEnabled(false);
    _ui->updateChannel->setCurrentText(updateChannelToLocalized(defaultUpdateChannel));
    setAndCheckNewUpdateChannel(defaultUpdateChannel);
}

void InfoSettings::slotRestoreUpdateChannel()
{
    restoreUpdateChannel();
}
#endif // defined(BUILD_UPDATER)

void InfoSettings::slotShowLegalNotice()
{
    auto notice = new LegalNotice();
    notice->exec();
    delete notice;
}

void InfoSettings::slotStyleChanged()
{
    customizeStyle();
}

void InfoSettings::customizeStyle()
{
    SettingsPanelStyle::apply(this);

    const auto aboutText = []() {
        auto aboutText = Theme::instance()->about();
        Theme::replaceLinkColorStringBackgroundAware(aboutText);
        return aboutText;
    }();
    _ui->infoAndUpdatesLabel->setText(aboutText);

#if defined(BUILD_UPDATER)
    slotUpdateInfo();
#else
    _ui->updatesContainer->setVisible(false);
    _ui->updatesGroupBox->setVisible(false);
#endif
}

} // namespace OCC
