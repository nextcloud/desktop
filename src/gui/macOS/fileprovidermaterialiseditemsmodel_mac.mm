/*
 * Copyright 2023 (c) Claudio Cambra <claudio.cambra@nextcloud.com>
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

#include "fileprovidermaterialiseditemsmodel.h"

#include <QLoggingCategory>

#import <FileProvider/FileProvider.h>

#include "fileproviderutils.h"

#include "gui/systray.h"

namespace OCC {

namespace Mac {

Q_LOGGING_CATEGORY(lcMacImplFileProviderMaterialisedItemsModelMac, "nextcloud.gui.macfileprovidermaterialiseditemsmodelmac", QtInfoMsg)

void FileProviderMaterialisedItemsModel::evictItem(const QString &identifier, const QString &domainIdentifier)
{
    NSFileProviderManager * const manager = FileProviderUtils::managerForDomainIdentifier(domainIdentifier);
    if (manager == nil) {
        qCWarning(lcMacImplFileProviderMaterialisedItemsModelMac) << "Received null manager for domain"
                                                                  << domainIdentifier
                                                                  << "cannot evict item"
                                                                  << identifier;
        Systray::instance()->showMessage(tr("Error"),
                                         tr("An internal error occurred. Please try again later."),
                                         QSystemTrayIcon::Warning);
        return;
    }

    [manager evictItemWithIdentifier:identifier.toNSString() completionHandler:^(NSError *error) {
        if (error != nil) {
            const auto errorDesc = QString::fromNSString(error.localizedDescription);
            qCWarning(lcMacImplFileProviderMaterialisedItemsModelMac) << "Error evicting item:" << errorDesc;
            Systray::instance()->showMessage(tr("Error"),
                                             tr("An error occurred while trying to delete the local copy of this item: %1").arg(errorDesc),
                                             QSystemTrayIcon::Warning);
        }
    }];

    // TODO: Update the model
}


} // namespace OCC

} // namespace Mac
