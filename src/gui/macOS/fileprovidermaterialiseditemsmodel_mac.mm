/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
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
    NSFileProviderManager *const manager = FileProviderUtils::managerForDomainIdentifier(domainIdentifier);
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

    __block BOOL successfullyDeleted = NO;
    dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);

    [manager evictItemWithIdentifier:identifier.toNSString() completionHandler:^(NSError *error) {
        if (error != nil) {
            const auto errorDesc = QString::fromNSString(error.localizedDescription);
            qCWarning(lcMacImplFileProviderMaterialisedItemsModelMac) << "Error evicting item:" << errorDesc;
            Systray::instance()->showMessage(tr("Error"),
                                             tr("An error occurred while trying to delete the local copy of this item: %1").arg(errorDesc),
                                             QSystemTrayIcon::Warning);
        } else {
            successfullyDeleted = YES;
        }
        dispatch_semaphore_signal(semaphore);
    }];

    dispatch_semaphore_wait(semaphore, dispatch_time(DISPATCH_TIME_NOW, 3 * NSEC_PER_SEC));
    [manager release];

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
