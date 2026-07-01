/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef GOVERNANCELABELINFO_H
#define GOVERNANCELABELINFO_H

#include <QString>
#include <QList>

namespace OCC
{

struct GovernanceLabelInfo
{
public:
    QString _id;

    QString _name;

    int _priority = -1;

    QString _description;

    QString _color;

    QStringList _scopes;
};

} // namespace OCC

#endif // GOVERNANCELABELINFO_H
