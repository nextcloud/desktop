/*
 * Copyright (C) 2023 by Claudio Cambra <claudio.cambra@nextcloud.com>
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

#include <QObject>

#import <Foundation/Foundation.h>

#import "ClientCommunicationProtocol.h"

#pragma once

namespace OCC::Mac {

class FileProviderXPC : public QObject
{
    Q_OBJECT

public:
    explicit FileProviderXPC(QObject *parent = nullptr);

public slots:
    void connectToExtensions();
    void configureExtensions();
    void authenticateExtension(const QString &extensionAccountId) const;
    void unauthenticateExtension(const QString &extensionAccountId) const;

private slots:
    void slotAccountStateChanged(AccountState::State state) const;

private:
    void setupConnections();
    void processConnections(NSArray *const services);

    NSDictionary<NSString *, NSObject<ClientCommunicationProtocol> *_clientCommServices;
};

} // namespace OCC::Mac
