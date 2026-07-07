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
    enum class Status {
        Selected,
        Available,
        UnknownStatus,
    };

    QString _id;

    QString _name;

    int _priority = -1;

    QString _description;

    QString _color;

    QStringList _scopes;

    Status _status = Status::UnknownStatus;
};

} // namespace OCC

#endif // GOVERNANCELABELINFO_H
