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

namespace OCC {

namespace Mac {

Q_LOGGING_CATEGORY(lcMacImplFileProviderMaterialisedItemsModelMac, "nextcloud.gui.macfileprovidermaterialiseditemsmodelmac", QtInfoMsg)

void FileProviderMaterialisedItemsModel::evictItem(const QString &identifier, const QString &domainIdentifier)
{
    NSFileProviderManager * const manager = FileProviderUtils::managerForDomainIdentifier(domainIdentifier);
    if (manager == nil) {
        qCWarning(lcMacImplFileProviderMaterialisedItemsModelMac) << "Received null manager for domain" << domainIdentifier
                                                                  << "cannot evict item" << identifier;
        return;
    }

    [manager evictItemWithIdentifier:identifier.toNSString() completionHandler:^(NSError *error) {
        if (error != nil) {
            qCWarning(lcMacImplFileProviderMaterialisedItemsModelMac) << "Error evicting item due to error:"
                                                                      << error.localizedDescription;
        }
    }];

    // TODO: Update the model
}


} // namespace OCC

} // namespace Mac
