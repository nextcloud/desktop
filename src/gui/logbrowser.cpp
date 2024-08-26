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

#include <cstdio>
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

LogBrowser::LogBrowser(QWidget *parent)
    : QDialog(parent)
{
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setObjectName("LogBrowser"); // for save/restoreGeometry()
    setWindowTitle(tr("Log Output"));
    setMinimumWidth(600);

    auto mainLayout = new QVBoxLayout;

    auto label = new QLabel(
        tr("The client can write debug logs to a temporary folder. "
           "These logs are very helpful for diagnosing problems.\n"
           "Since log files can get large, the client will start a new one for each sync "
           "run and compress older ones. It will also delete log files after a couple "
           "of hours to avoid consuming too much disk space.\n"
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

    label = new QLabel(
        tr("This setting persists across client restarts.\n"
           "Note that using any logging command line options will override this setting."));
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

    auto *btnbox = new QDialogButtonBox;
    QPushButton *closeBtn = btnbox->addButton(QDialogButtonBox::Close);
    connect(closeBtn, &QAbstractButton::clicked, this, &QWidget::close);

    mainLayout->addStretch();
    mainLayout->addWidget(btnbox);

    setLayout(mainLayout);

    setModal(false);

    auto showLogWindow = new QAction(this);
    showLogWindow->setShortcut(QKeySequence("F12"));
    connect(showLogWindow, &QAction::triggered, this, &QWidget::close);
    addAction(showLogWindow);

    ConfigFile cfg;
    cfg.restoreGeometry(this);
}

LogBrowser::~LogBrowser() = default;

void LogBrowser::closeEvent(QCloseEvent *)
{
    ConfigFile cfg;
    cfg.saveGeometry(this);
}

void LogBrowser::togglePermanentLogging(bool enabled)
{
    ConfigFile().setAutomaticLogDir(enabled);

    auto logger = Logger::instance();
    if (enabled) {
        if (!logger->isLoggingToFile()) {
            logger->setupTemporaryFolderLogDir();
            logger->enterNextLogFile(QStringLiteral("nextcloud.log"), OCC::Logger::LogType::Log);
        }
    } else {
        logger->disableTemporaryFolderLogDir();
    }
}

} // namespace
