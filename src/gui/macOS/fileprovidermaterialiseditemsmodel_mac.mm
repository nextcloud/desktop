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

    __block BOOL successfullyDeleted = YES;

    [manager evictItemWithIdentifier:identifier.toNSString() completionHandler:^(NSError *error) {
        if (error != nil) {
            const auto errorDesc = QString::fromNSString(error.localizedDescription);
            qCWarning(lcMacImplFileProviderMaterialisedItemsModelMac) << "Error evicting item:" << errorDesc;
            Systray::instance()->showMessage(tr("Error"),
                                             tr("An error occurred while trying to delete the local copy of this item: %1").arg(errorDesc),
                                             QSystemTrayIcon::Warning);
            successfullyDeleted = NO;
        }
    }];

    if (successfullyDeleted == NO) {
        return;
    }

    const auto deletedItemIt = std::find_if(_items.cbegin(),
                                            _items.cend(),
                                            [identifier, domainIdentifier](const FileProviderItemMetadata &item) {
        return item.identifier() == identifier && item.domainIdentifier() == domainIdentifier;
    });

    if (deletedItemIt == _items.cend()) {
        qCWarning(lcMacImplFileProviderMaterialisedItemsModelMac) << "Could not find item"
                                                                  << identifier
                                                                  << "in model items.";
        return;
    }

    const auto deletedItemRow = std::distance(_items.cbegin(), deletedItemIt);
    beginRemoveRows({}, deletedItemRow, deletedItemRow);
    _items.remove(deletedItemRow);
    endRemoveRows();
}


} // namespace OCC

} // namespace Mac
