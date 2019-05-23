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

namespace OCC {

// ==============================================================================

const std::chrono::hours defaultExpireDuration(4);

LogBrowser::LogBrowser(QWidget *parent)
    : QDialog(parent)
{
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setObjectName("LogBrowser"); // for save/restoreGeometry()
    setWindowTitle(tr("Log Output"));
    setMinimumWidth(600);

    QVBoxLayout *mainLayout = new QVBoxLayout;

    auto label = new QLabel(
        tr("The client can write debug logs to a temporary folder. "
           "These logs are very helpful for diagnosing problems.\n"
           "Since log files can get large, the client will start a new one for each sync "
           "run and compress older ones. It is also recommended to enable deleting log files "
           "after a couple of hours to avoid consuming too much disk space.\n"
           "If enabled, logs will be written to %1")
        .arg(Logger::instance()->temporaryFolderLogDirPath()));
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);
    mainLayout->addWidget(label);

    // button to permanently save logs
    auto enableLoggingButton = new QCheckBox;
    enableLoggingButton->setText(tr("Enable logging to temporary folder"));
    enableLoggingButton->setChecked(ConfigFile().automaticLogDir());
    connect(enableLoggingButton, &QCheckBox::toggled, this, &LogBrowser::togglePermanentLogging);
    mainLayout->addWidget(enableLoggingButton);

    auto deleteLogsButton = new QCheckBox;
    deleteLogsButton->setText(tr("Delete logs older than %1 hours").arg(QString::number(defaultExpireDuration.count())));
    deleteLogsButton->setChecked(bool(ConfigFile().automaticDeleteOldLogsAge()));
    connect(deleteLogsButton, &QCheckBox::toggled, this, &LogBrowser::toggleLogDeletion);
    mainLayout->addWidget(deleteLogsButton);

    label = new QLabel(
        tr("These settings persist across client restarts.\n"
           "Note that using any logging command line options will override the settings."));
    label->setWordWrap(true);
    label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);
    mainLayout->addWidget(label);

    auto openFolderButton = new QPushButton;
    openFolderButton->setText(tr("Open folder"));
    connect(openFolderButton, &QPushButton::clicked, this, []() {
        QString path = Logger::instance()->temporaryFolderLogDirPath();
        QDir().mkpath(path);
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    });
    mainLayout->addWidget(openFolderButton);

    QDialogButtonBox *btnbox = new QDialogButtonBox;
    QPushButton *closeBtn = btnbox->addButton(QDialogButtonBox::Close);
    connect(closeBtn, &QAbstractButton::clicked, this, &QWidget::close);

    mainLayout->addStretch();
    mainLayout->addWidget(btnbox);

    setLayout(mainLayout);

    setModal(false);

    QAction *showLogWindow = new QAction(this);
    showLogWindow->setShortcut(QKeySequence("F12"));
    connect(showLogWindow, &QAction::triggered, this, &QWidget::close);
    addAction(showLogWindow);

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
        logger->enterNextLogFile();
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
