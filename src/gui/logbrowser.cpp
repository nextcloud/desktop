/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
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

#include "logbrowser.h"

#include "stdio.h"
#include <iostream>

#include <QDialogButtonBox>
#include <QLayout>
#include <QPushButton>
#include <QLabel>
#include <QDir>
#include <QTextStream>
#include <QMessageBox>
#include <QCoreApplication>
#include <QSettings>
#include <QAction>
#include <QDesktopServices>

#include "configfile.h"
#include "logger.h"
#include "guiutility.h"
#include "ui_logbrowser.h"

namespace OCC {

// ==============================================================================

const std::chrono::hours defaultExpireDuration(4);

LogBrowser::LogBrowser(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::LogBrowser)
{
    ui->setupUi(this);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    ui->warningLabel->setPixmap(Utility::getCoreIcon(QStringLiteral("warning")).pixmap(ui->warningLabel->size()));
    ui->locationLabel->setText(Logger::instance()->temporaryFolderLogDirPath());

    ui->enableLoggingButton->setChecked(ConfigFile().automaticLogDir());
    connect(ui->enableLoggingButton, &QCheckBox::toggled, this, &LogBrowser::togglePermanentLogging);

    ui->httpLogButton->setChecked(ConfigFile().logHttp());
    connect(ui->httpLogButton, &QCheckBox::toggled, this, [](bool b) {
        ConfigFile().setLogHttp(b);
    });

    ui->deleteLogsButton->setText(tr("Delete logs older than %1 hours").arg(QString::number(defaultExpireDuration.count())));
    ui->deleteLogsButton->setChecked(bool(ConfigFile().automaticDeleteOldLogsAge()));
    connect(ui->deleteLogsButton, &QCheckBox::toggled, this, &LogBrowser::toggleLogDeletion);


    connect(ui->openFolderButton, &QPushButton::clicked, this, []() {
        QString path = Logger::instance()->temporaryFolderLogDirPath();
        QDir().mkpath(path);
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    });
    connect(ui->buttonBox->button(QDialogButtonBox::Close), &QPushButton::clicked, this, &QWidget::close);

    ConfigFile cfg;
    cfg.restoreGeometry(this);
}

LogBrowser::~LogBrowser()
{
}

void LogBrowser::setupLoggingFromConfig()
{
    ConfigFile config;
    auto logger = Logger::instance();

    if (config.automaticLogDir()) {
        // Don't override other configured logging
        if (logger->isLoggingToFile())
            return;

        logger->setupTemporaryFolderLogDir();
        if (auto deleteOldLogsHours = config.automaticDeleteOldLogsAge()) {
            logger->setLogExpire(*deleteOldLogsHours);
        } else {
            logger->setLogExpire(std::chrono::hours(0));
        }
    } else {
        logger->disableTemporaryFolderLogDir();
    }
}

void LogBrowser::togglePermanentLogging(bool enabled)
{
    ConfigFile config;
    config.setAutomaticLogDir(enabled);
    setupLoggingFromConfig();
}

void LogBrowser::toggleLogDeletion(bool enabled)
{
    ConfigFile config;
    auto logger = Logger::instance();

    if (enabled) {
        config.setAutomaticDeleteOldLogsAge(defaultExpireDuration);
        logger->setLogExpire(defaultExpireDuration);
    } else {
        config.setAutomaticDeleteOldLogsAge({});
        logger->setLogExpire(std::chrono::hours(0));
    }
}

} // namespace
