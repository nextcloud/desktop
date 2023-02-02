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

namespace OCC {

class AccountState;

namespace Mac {

class FileProviderDomainManager : public QObject
{
    Q_OBJECT

public:
    static FileProviderDomainManager *instance();
    ~FileProviderDomainManager() override;

private slots:
    void setupFileProviderDomains();

    void addFileProviderDomainForAccount(OCC::AccountState *accountState);
    void removeFileProviderDomainForAccount(OCC::AccountState *accountState);

private:
    explicit FileProviderDomainManager(QObject *parent = nullptr);
    static FileProviderDomainManager *_instance;
    class Private;
    QScopedPointer<Private> d;

};

} // namespace Mac

} // namespace OCC
