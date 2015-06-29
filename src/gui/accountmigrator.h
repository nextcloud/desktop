/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#ifndef ACCOUNTMIGRATOR_H
#define ACCOUNTMIGRATOR_H

#include <QStringList>

namespace OCC {

/**
 * @brief The AccountSettings class
 * @ingroup gui
 */
class AccountMigrator {

public:
    explicit AccountMigrator();

    /**
     * @brief migrateFolderDefinitons - migrate the folder definition files
     * @return the list of migrated folder definitions
     */
    QStringList migrateFolderDefinitons();

};
}

#endif // ACCOUNTMIGRATOR_H
