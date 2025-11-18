/*
 * SPDX-FileCopyrightText: 2017 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
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
    void slotFolderListChanged(const OCC::Folder::Map &folderMap);

private:
    QMap<QString, CloudProviderWrapper*> _map;
    unsigned int _folder_index;
};

#endif // CLOUDPROVIDERMANAGER_H
