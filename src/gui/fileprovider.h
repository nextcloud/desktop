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

#ifndef FILEPROVIDER_H
#define FILEPROVIDER_H

#include <QObject>
#include <QScopedPointer>

#include "accountstate.h"

namespace OCC {
namespace Mac {

class FileProvider : public QObject
{
public:
    static FileProvider *instance();
    ~FileProvider() override;

    void setupFileProviderDomains();

public slots:
    void addFileProviderDomainForAccount(AccountState *account);
    void removeFileProviderDomainForAccount(AccountState *account);

private:
    explicit FileProvider(QObject *parent = nullptr);
    static FileProvider *_instance;
    class Private;
    QScopedPointer<Private> d;
};

} // namespace Mac
} // namespace OCC

#endif
