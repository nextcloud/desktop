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

namespace OCC {

namespace Mac {

class FileProviderSocketController;
using FileProviderSocketControllerPtr = QPointer<FileProviderSocketController>;

QString fileProviderSocketPath();

class FileProviderSocketServer : public QObject
{
    Q_OBJECT

public:
    explicit FileProviderSocketServer(QObject *parent = nullptr);

private slots:
    void startListening();
    void slotNewConnection();
    void slotSocketDestroyed(const QLocalSocket * const socket);

private:
    QString _socketPath;
    QLocalServer _socketServer;
    QHash<const QLocalSocket*, FileProviderSocketControllerPtr> _socketControllers;
};

} // namespace Mac

} // namespace OCC
