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

#include "common/restartmanager.h"
#include "gui/guiutility.h"
#include "libsync/configfile.h"
#include "libsync/theme.h"

#ifdef WITH_AUTO_UPDATER
#include "updater/ocupdater.h"
#ifdef Q_OS_MAC
// FIXME We should unify those, but Sparkle does everything behind the scene transparently
#include "updater/sparkleupdater.h"
#endif
#endif

namespace {
#ifdef WITH_AUTO_UPDATER
bool isTestPilotCloudTheme()
{
    return OCC::Theme::instance()->appName() == QLatin1String("testpilotcloud");
}
#endif
}

namespace OCC {

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::AboutDialog)
{
    ui->setupUi(this);
    ui->aboutText->setText(Theme::instance()->about());
    ui->icon->setPixmap(Theme::instance()->aboutIcon().pixmap(256));
    ui->versionInfo->setText(Theme::instance()->aboutVersions(Theme::VersionFormat::RichText));

    connect(ui->versionInfo, &QTextBrowser::anchorClicked, this, &AboutDialog::openBrowserFromUrl);
    connect(ui->aboutText, &QLabel::linkActivated, this, &AboutDialog::openBrowser);

    setupUpdaterWidget();
}

AboutDialog::~AboutDialog()
{
    delete ui;
}

void AboutDialog::openBrowser(const QString &s)
{
    Utility::openBrowser(QUrl(s), this);
}

void AboutDialog::openBrowserFromUrl(const QUrl &s)
{
    return openBrowser(s.toString());
}

void AboutDialog::setupUpdaterWidget()
{
#ifdef WITH_AUTO_UPDATER
    // non standard update channels are only supported by the vanilla theme and the testpilotcloud theme
    if (!Resources::isVanillaTheme() && !isTestPilotCloudTheme()) {
        if (Utility::isMac()) {
            // Because we don't have any statusString from the SparkleUpdater anyway we can hide the whole thing
            ui->updaterWidget->hide();
        } else {
            ui->updateChannelLabel->hide();
            ui->updateChannel->hide();
            if (ConfigFile().updateChannel() != QLatin1String("stable")) {
                ConfigFile().setUpdateChannel(QStringLiteral("stable"));
            }
        }
    }
    // we want to attach the known english identifiers which are also used within the configuration file as user data inside the data model
    // that way, when we intend to reset to the original selection when the dialog, we can look up the config file's stored value in the data model
    ui->updateChannel->addItem(tr("ownCloud 10 LTS"), QStringLiteral("stable"));
    ui->updateChannel->addItem(tr("ownCloud Infinite Scale stable"), QStringLiteral("ocis"));
    if (!Resources::isVanillaTheme()) {
        ui->updateChannel->addItem(tr("beta"), QStringLiteral("beta"));
    }

    if (!ConfigFile().skipUpdateCheck() && Updater::instance()) {
        // Channel selection
        ui->updateChannel->setCurrentIndex(ui->updateChannel->findData(ConfigFile().updateChannel()));
        connect(ui->updateChannel, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &AboutDialog::slotUpdateChannelChanged);

        // Note: the sparkle-updater is not an OCUpdater
        if (auto *ocupdater = qobject_cast<OCUpdater *>(Updater::instance())) {
            auto updateInfo = [ocupdater, this] {
                QString statusString = ocupdater->statusString();
                switch (ocupdater->downloadState()) {
                case OCUpdater::Unknown:
                    [[fallthrough]];
                case OCUpdater::CheckingServer:
                    [[fallthrough]];
                case OCUpdater::UpToDate:
                    // No update, leave the status string as is.
                    break;
                case OCUpdater::Downloading:
                    [[fallthrough]];
                case OCUpdater::DownloadComplete:
                    [[fallthrough]];
                case OCUpdater::DownloadFailed:
                    [[fallthrough]];
                case OCUpdater::DownloadTimedOut:
                    [[fallthrough]];
                case OCUpdater::UpdateOnlyAvailableThroughSystem:
                    statusString = QStringLiteral("Version %1 is available. %2").arg(ocupdater->availableVersionString(), statusString);
                    break;
                }

                ui->updateStateLabel->setText(statusString);
                ui->restartButton->setVisible(ocupdater->downloadState() == OCUpdater::DownloadComplete);
            };
            connect(ocupdater, &OCUpdater::downloadStateChanged, this, updateInfo);
            if (auto *nsisupdater = qobject_cast<WindowsUpdater *>(ocupdater)) {
                connect(ui->restartButton, &QAbstractButton::clicked, nsisupdater, &WindowsUpdater::startInstallerAndQuit);
            } else {
                connect(ui->restartButton, &QAbstractButton::clicked, this, [] { RestartManager::requestRestart(); });
            }
            updateInfo();
        }
#ifdef HAVE_SPARKLE
        if (SparkleUpdater *sparkleUpdater = qobject_cast<SparkleUpdater *>(Updater::instance())) {
            ui->updateStateLabel->setText(sparkleUpdater->statusString());
            ui->restartButton->setVisible(false);
        }
#endif
    } else {
        ui->updaterWidget->hide();
    }
#else
    ui->updaterWidget->hide();
#endif
}

void AboutDialog::slotUpdateChannelChanged([[maybe_unused]] int index)
{
#ifdef WITH_AUTO_UPDATER
    QString channel;
    if (index < 0) {
        // invalid index, reset to stable
        channel = QStringLiteral("stable");
    } else {
        channel = ui->updateChannel->itemData(index).toString();
    }
    if (channel == ConfigFile().updateChannel()) {
        return;
    }

    auto msgBox = new QMessageBox(QMessageBox::Warning, tr("Change update channel?"),
        tr("<html>The update channel determines which client updates will be offered for installation.<ul>"
           "<li>\"ownCloud 10 LTS\" contains only upgrades that are considered reliable</li>"
           "<li>\"ownCloud Infinite Scale stable\" contains only upgrades that are considered reliable but <b>removes support for \"ownCloud 10\"</b></li>"
           "%1"
           "</ul>"
           "<br>⚠️Downgrades are not supported. If you switch to a stable channel this change will only be applied with the next major release.</html>")
            .arg(
                isTestPilotCloudTheme() ? tr("<li>\"beta\" may contain newer features and bugfixes, but have not yet been tested thoroughly</li>") : QString()),
        QMessageBox::NoButton, this);
    auto acceptButton = msgBox->addButton(tr("Change update channel"), QMessageBox::AcceptRole);
    msgBox->addButton(tr("Cancel"), QMessageBox::RejectRole);
    connect(msgBox, &QMessageBox::finished, msgBox, [this, channel, msgBox, acceptButton] {
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
            const auto oldChannel = ui->updateChannel->findData(ConfigFile().updateChannel());
            Q_ASSERT(oldChannel >= 0);
            Q_ASSERT(oldChannel <= 1);
            ui->updateChannel->setCurrentIndex(oldChannel);
        }
    });
    msgBox->open();
#endif
}

} // OCC namespace
