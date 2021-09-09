/*
 * Copyright (C) by Oleksandr Zolotov <alex@nextcloud.com>
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

#include <QtCore>

#include "UnifiedSearchResult.h"

namespace OCC {
bool operator<(const UnifiedSearchResult &rhs, const UnifiedSearchResult &lhs)
{
    return (rhs._order > lhs._order && rhs._categoryId > lhs._categoryId && rhs._title > lhs._title && rhs._subline > lhs._subline && rhs._resourceUrl > lhs._resourceUrl && (rhs._isFetchMoreTrigger && !lhs._isFetchMoreTrigger));
}

bool operator==(const UnifiedSearchResult &rhs, const UnifiedSearchResult &lhs)
{
    return (rhs._order == lhs._order && rhs._categoryId == lhs._categoryId && rhs._title == lhs._title && rhs._subline == lhs._subline && rhs._resourceUrl == lhs._resourceUrl);
}
}
