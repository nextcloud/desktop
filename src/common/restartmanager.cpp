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

#include "restartmanager.h"

#include "utility.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QProcess>
#include <QTimer>

Q_LOGGING_CATEGORY(lcRestart, "sync.restartmanager", QtInfoMsg)

using namespace OCC;

RestartManager *RestartManager::_instance = nullptr;

RestartManager::RestartManager(std::function<int(int, char **)> &&main)
    : _main(main)
{
    Q_ASSERT(!_instance);
    _instance = this;
}

RestartManager::~RestartManager()
{
    if (!_applicationToRestart.isEmpty()) {
        QProcess process;
        process.setProgram(_applicationToRestart);
        process.setArguments(_args);
        qint64 pid;
        qCDebug(lcRestart) << "Detaching" << _applicationToRestart << _args;
        if (process.startDetached(&pid)) {
            qCDebug(lcRestart) << "Successfully restarted. New process PID" << pid;
        } else {
            qCCritical(lcRestart) << "Failed to restart" << process.error() << process.errorString();
        }
    }
}

int RestartManager::exec(int argc, char **argv) const
{
    return _main(argc, argv);
}

void RestartManager::requestRestart()
{
    Q_ASSERT(_instance);
    qCInfo(lcRestart) << "Restarting application with PID" << QCoreApplication::applicationPid();

    QString pathToLaunch = QCoreApplication::applicationFilePath();
#ifdef Q_OS_LINUX
    if (Utility::runningInAppImage()) {
        pathToLaunch = Utility::appImageLocation();
    }
#endif
    _instance->_applicationToRestart = QFileInfo(pathToLaunch).absoluteFilePath();
    // remove arg0
    _instance->_args = QCoreApplication::arguments().sliced(1);
    QTimer::singleShot(0, QCoreApplication::instance(), &QCoreApplication::quit);
}
