/*
 * Copyright (C) by Julius Härtl <jus@bitgrid.net>
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

#ifndef CLOUDPROVIDERMANAGER_H
#define CLOUDPROVIDERMANAGER_H

#include <QObject>
#include "folder.h"

using namespace OCC;

class CloudProviderWrapper;

class CloudProviderManager : public QObject
{
    Q_OBJECT
public:
    explicit CloudProviderManager(QObject *parent = nullptr);
    void registerSignals();

signals:

public slots:
    void slotFolderListChanged(const Folder::Map &folderMap);

private:
    QMap<QString, CloudProviderWrapper*> *_map;
};

#endif // CLOUDPROVIDERMANAGER_H
