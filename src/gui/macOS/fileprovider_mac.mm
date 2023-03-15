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

#import <Foundation/Foundation.h>

#include <QLoggingCategory>

#include "fileprovider.h"
#include "fileproviderdomainmanager.h"

namespace OCC {

Q_LOGGING_CATEGORY(lcMacFileProvider, "nextcloud.gui.macfileprovider", QtInfoMsg)

namespace Mac {

static FileProvider *_instance = nullptr;

FileProvider::FileProvider(QObject * const parent)
    : QObject(parent)
{
    const auto domainManager = FileProviderDomainManager::instance();
    if (domainManager) {
        qCDebug(lcMacFileProvider()) << "Initialized file provider domain manager";
    }
}

FileProvider *FileProvider::instance()
{
    if (!_instance) {
        _instance = new FileProvider();
    }
    return _instance;
}

bool FileProvider::fileProviderAvailable()
{
    if (@available(macOS 11.0, *)) {
        return true;
    }

    return false;
}

} // namespace Mac
} // namespace OCC
