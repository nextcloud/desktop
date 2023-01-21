/*
 * Copyright (C) by Roeland Jago Douma <roeland@owncloud.com>
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

#ifndef SHAREE_H
#define SHAREE_H

#include <QObject>
#include <QFlags>
#include <QAbstractListModel>
#include <QLoggingCategory>
#include <QModelIndex>
#include <QVariant>
#include <QSharedPointer>
#include <QVector>

#include "accountfwd.h"

class QJsonDocument;
class QJsonObject;

namespace OCC {

Q_DECLARE_LOGGING_CATEGORY(lcSharing)

class Sharee
{
public:
    // Keep in sync with Share::ShareType
    enum Type {
        User = 0,
        Group = 1,
        Email = 4,
        Federated = 6,
        Circle = 7,
        Room = 10
    };

    explicit Sharee(const QString shareWith,
        const QString displayName,
        const Type type);

    [[nodiscard]] QString format() const;
    [[nodiscard]] QString shareWith() const;
    [[nodiscard]] QString displayName() const;
    [[nodiscard]] Type type() const;

private:
    QString _shareWith;
    QString _displayName;
    Type _type;
};

using ShareePtr = QSharedPointer<OCC::Sharee>;
}

Q_DECLARE_METATYPE(OCC::ShareePtr)

#endif //SHAREE_H
