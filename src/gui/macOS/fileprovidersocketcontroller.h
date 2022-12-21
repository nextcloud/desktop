/*
 * Copyright (C) 2022 by Claudio Cambra <claudio.cambra@nextcloud.com>
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

#pragma once

#include <QObject>
#include <QLocalServer>
#include <QLoggingCategory>

namespace OCC
{

namespace Mac {

#ifdef Q_OS_MACOS
QString fileProviderSocketPath();
#endif

class FileProviderSocketController : public QObject
{
    Q_OBJECT

public:
    explicit FileProviderSocketController(QObject *parent = nullptr);

private slots:
    void startListening();
    void slotNewConnection();

private:
    QString _socketPath;
    QLocalServer _socketServer;

};

} // namespace Mac

} // namespace OCC
