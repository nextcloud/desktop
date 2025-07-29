/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "fileprovider.h"

#include <QLoggingCategory>

#include "libsync/configfile.h"
#include "gui/macOS/fileproviderxpc.h"

#import <Foundation/Foundation.h>

namespace OCC {

Q_LOGGING_CATEGORY(lcMacFileProvider, "nextcloud.gui.macfileprovider", QtInfoMsg)

namespace Mac {

FileProvider* FileProvider::_instance = nullptr;

FileProvider::FileProvider(QObject * const parent)
    : QObject(parent)
{
    Q_ASSERT(!_instance);

    if (!fileProviderAvailable()) {
        qCInfo(lcMacFileProvider) << "File provider system is not available on this version of macOS.";
        deleteLater();
        return;
    }

    qCInfo(lcMacFileProvider) << "Initialising file provider domain manager.";
    _domainManager = std::make_unique<FileProviderDomainManager>(this);

    if (_domainManager) {
        connect(_domainManager.get(), &FileProviderDomainManager::domainSetupComplete, this, &FileProvider::configureXPC);
        _domainManager->start();
        qCDebug(lcMacFileProvider()) << "Initialized file provider domain manager";
    }

    qCDebug(lcMacFileProvider) << "Initialising file provider socket server.";
    _socketServer = std::make_unique<FileProviderSocketServer>(this);

    if (_socketServer) {
        qCDebug(lcMacFileProvider) << "Initialised file provider socket server.";
    }
}

FileProvider *FileProvider::instance()
{
    if (!fileProviderAvailable()) {
        qCInfo(lcMacFileProvider) << "File provider system is not available on this version of macOS.";
        return nullptr;
    }

    if (!_instance) {
        _instance = new FileProvider();
    }
    return _instance;
}

FileProvider::~FileProvider()
{
    _instance = nullptr;
}

bool FileProvider::fileProviderAvailable()
{
    if (@available(macOS 11.0, *)) {
        return true;
    }

    return false;
}

void FileProvider::configureXPC()
{
    _xpc = std::make_unique<FileProviderXPC>(new FileProviderXPC(this));
    if (_xpc) {
        qCInfo(lcMacFileProvider) << "Initialised file provider XPC.";
        _xpc->connectToFileProviderDomains();
        _xpc->authenticateFileProviderDomains();
    } else {
        qCWarning(lcMacFileProvider) << "Could not initialise file provider XPC.";
    }
}

FileProviderXPC *FileProvider::xpc() const
{
    return _xpc.get();
}

FileProviderDomainManager *FileProvider::domainManager() const
{
    return _domainManager.get();
}

FileProviderSocketServer *FileProvider::socketServer() const
{
    return _socketServer.get();
}

} // namespace Mac
} // namespace OCC
