/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QObject>

#include "fileproviderdomainmanager.h"
#include "fileproviderservice.h"
#include "fileprovidersocketserver.h"
#include "fileproviderxpc.h"

namespace OCC {

class Application;

namespace Mac {

// NOTE: For the file provider extension to work, the app bundle will
// need to be correctly codesigned!

class FileProvider : public QObject
{
    Q_OBJECT

public:
    static FileProvider *instance();
    ~FileProvider() override;

    void configureXPC();
    [[nodiscard]] FileProviderXPC *xpc() const;
    [[nodiscard]] FileProviderDomainManager *domainManager() const;
    [[nodiscard]] FileProviderSocketServer *socketServer() const;
    [[nodiscard]] FileProviderService *service() const;

private:
    std::unique_ptr<FileProviderDomainManager> _domainManager;
    std::unique_ptr<FileProviderSocketServer> _socketServer;
    std::unique_ptr<FileProviderXPC> _xpc;
    std::unique_ptr<FileProviderService> _service;

    static FileProvider *_instance;
    explicit FileProvider(QObject * const parent = nullptr);

    friend class OCC::Application;
};

} // namespace Mac
} // namespace OCC
