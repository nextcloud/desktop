/*
 * Copyright (C) by Claudio Cambra <claudio.cambra@nextcloud.com>
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

#include <QJsonArray>
#include <QVariantMap>

#pragma once

class QByteArray;
class QJsonValue;

class FakeRemoteActivityStorage
{
    FakeRemoteActivityStorage() = default;

public:
    static FakeRemoteActivityStorage *instance();

    static void destroy();

    void init();
    void initActivityData();

    [[nodiscard]] QByteArray activityJsonData(const int sinceId, const int limit);

    [[nodiscard]] QJsonValue activityById(const int id) const;

    [[nodiscard]] int startingIdLast() const;

private:
    QJsonArray _activityData;
    QVariantMap _metaSuccess;
    quint32 _numItemsToInsert = 30;
    int _startingId = 90000;

    static FakeRemoteActivityStorage *_instance;
};
