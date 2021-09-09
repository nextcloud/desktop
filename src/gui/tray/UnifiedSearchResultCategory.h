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

#ifndef UNIFIEDSEARHRESULTCATEGORY_H
#define UNIFIEDSEARHRESULTCATEGORY_H

#include "UnifiedSearchResult.h"

#include <QtCore>
#include <QIcon>

namespace OCC {
/**
 * @brief The UnifiedSearchResultCategory class
 */

class UnifiedSearchResultCategory
{
    Q_GADGET

    Q_PROPERTY(QString name MEMBER _name)
    Q_PROPERTY(QList<UnifiedSearchResult> results MEMBER _results)

public:
    QString _id;
    QString _name;
    qint32 _order = INT32_MAX;
    qint32 _cursor = -1;
    bool _isPaginated = false;
    QList<UnifiedSearchResult> _results;
};

bool operator==(const UnifiedSearchResultCategory &rhs, const UnifiedSearchResultCategory &lhs);
bool operator<(const UnifiedSearchResultCategory &rhs, const UnifiedSearchResultCategory &lhs);
}

Q_DECLARE_METATYPE(OCC::UnifiedSearchResultCategory)

#endif // UNIFIEDSEARHRESULTCATEGORY_H
