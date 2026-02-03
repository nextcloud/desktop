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
    qCDebug(lcMacFileProvider) << "Initializing...";

    _domainManager = std::make_unique<FileProviderDomainManager>(this);

    if (_domainManager) {
        _domainManager->start();
    }

    _socketServer = std::make_unique<FileProviderSocketServer>(this);

    if (_socketServer) {
        qCDebug(lcMacFileProvider) << "Initialised file provider socket server.";
    }

    _service = std::make_unique<FileProviderService>(this);

    if (_service) {
        qCDebug(lcMacFileProvider) << "Initialised file provider service.";
    }
}

FileProvider *FileProvider::instance()
{
    if (!_instance) {
        _instance = new FileProvider();
    }

    return _instance;
}

FileProvider::~FileProvider()
{
    _instance = nullptr;
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

FileProviderService *FileProvider::service() const
{
    return _service.get();
}

} // namespace Mac
} // namespace OCC
