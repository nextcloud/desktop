/*
 * Copyright (C) by Claudio Cambra <claudio.cambra@nextcloud.com>
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

#include "fileproviderdomainmanager.h"
#include "fileprovidersocketserver.h"

namespace OCC {

namespace Mac {

// NOTE: For the file provider extension to work, the app bundle will
// need to be correctly codesigned!

class FileProvider : public QObject
{
    Q_OBJECT

public:
    static FileProvider *instance();
    ~FileProvider() = default;

    static bool fileProviderAvailable();

private:
    explicit FileProvider(QObject * const parent = nullptr);
    std::unique_ptr<FileProviderDomainManager> _domainManager;
    std::unique_ptr<FileProviderSocketServer> _socketServer;
};

} // namespace Mac
} // namespace OCC
