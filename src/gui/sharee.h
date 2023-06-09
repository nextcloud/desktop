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
    Q_GADGET
    Q_PROPERTY(QString format READ format)
    Q_PROPERTY(QString shareWith MEMBER _shareWith)
    Q_PROPERTY(QString displayName MEMBER _displayName)
    Q_PROPERTY(QString iconUrlColoured MEMBER _iconUrlColoured)
    Q_PROPERTY(Type type MEMBER _type)

public:
    // Keep in sync with Share::ShareType
    enum Type { Invalid = -1, User = 0, Group = 1, Email = 4, Federated = 6, Circle = 7, Room = 10, LookupServerSearch = 999, LookupServerSearchResults = 1000 };
    Q_ENUM(Type);
    explicit Sharee() = default;
    explicit Sharee(const QString &shareWith, const QString &displayName, const Type type, const QString &iconUrl = {});

    [[nodiscard]] QString format() const;
    [[nodiscard]] QString shareWith() const;
    [[nodiscard]] QString displayName() const;
    [[nodiscard]] QString iconUrl() const;
    [[nodiscard]] QString iconUrlColoured() const;
    [[nodiscard]] Type type() const;
    bool updateIconUrl();

    void setDisplayName(const QString &displayName);
    void setType(const Type &type);
    void setIsIconColourful(const bool isColourful);
    void setIconUrl(const QString &iconUrl);

private:
    QString _shareWith;
    QString _displayName;
    QString _iconUrlColoured;
    QString _iconColor;
    Type _type = Type::Invalid;
    QString _iconUrl;
    bool _isIconColourful = false;
};

using ShareePtr = QSharedPointer<OCC::Sharee>;
}

Q_DECLARE_METATYPE(OCC::ShareePtr)
Q_DECLARE_METATYPE(OCC::Sharee)

#endif //SHAREE_H
