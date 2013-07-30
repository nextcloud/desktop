/*
 * Copyright (C) by Duncan Mac-Vicar P. <duncan@kde.org>
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

#ifndef MIRALL_FILEUTILS_H
#define MIRALL_FILEUTILS_H

#include <QStringList>

namespace Mirall
{

class FileUtils
{
public:
    enum SubFolderListOption {
        SubFolderNoOptions = 0x0,
        SubFolderRecursive = 0x1
    };
    Q_DECLARE_FLAGS(SubFolderListOptions, SubFolderListOption)

    static QStringList subFoldersList(QString folder,
                                      SubFolderListOptions options = SubFolderNoOptions );
    static bool removeDir(const QString &path);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(FileUtils::SubFolderListOptions)

}

#endif
