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

#include "gui/tray/activitylistmodel.h"

#include "libsync/account.h"
#include "gui/accountstate.h"
#include "gui/accountmanager.h"

#pragma once

class FakeQNAM;
class QByteArray;
class QJsonValue;

namespace ActivityListModelTestUtils
{

[[nodiscard]] QNetworkReply *almTestQnamOverride(FakeQNAM * const fakeQnam,
                                                 const QNetworkAccessManager::Operation op,
                                                 const QNetworkRequest &req,
                                                 const QString &accountUrl,
                                                 QObject * const parent = nullptr,
                                                 const int searchResultsReplyDelay = 0,
                                                 QIODevice * const device = nullptr);

[[nodiscard]] OCC::Activity exampleNotificationActivity(const QString &accountName, const int id = 1);
[[nodiscard]] OCC::Activity exampleSyncResultErrorActivity(const QString &accountName, const int id = 2);
[[nodiscard]] OCC::Activity exampleSyncFileItemActivity(const QString &accountName, const QUrl &link, const int id = 3);
[[nodiscard]] OCC::Activity exampleFileIgnoredActivity(const QString &accountName, const QUrl &link = {}, const int id = 4);

class FakeRemoteActivityStorage
{
    FakeRemoteActivityStorage() = default;

public:
    static FakeRemoteActivityStorage *instance();

    [[nodiscard]] QByteArray activityJsonData(const int sinceId, const int limit);
    [[nodiscard]] QJsonValue activityById(const int id) const;

    [[nodiscard]] int startingIdLast() const;
    [[nodiscard]] int numItemsToInsert() const;
    [[nodiscard]] int totalNumActivites() const;

    static void destroy();
    void init();
    void initActivityData();

private:
    QJsonArray _activityData;
    QVariantMap _metaSuccess;
    quint32 _numItemsToInsert = 10;
    int _startingId = 90000;

    static FakeRemoteActivityStorage *_instance;
};

class TestingALM : public OCC::ActivityListModel
{
    Q_OBJECT

public:
    TestingALM() = default;

    [[nodiscard]] int maxActivities() const
    {
        return _maxActivities;
    };
    // Need to include the dummy "show more in activities app" activity
    [[nodiscard]] int maxPossibleActivities() const
    {
        return maxActivities() + 1;
    }

public slots:
    void startFetchJob() override;
    void startMaxActivitiesFetchJob();
    void slotProcessReceivedActivities();

signals:
    void activitiesProcessed();

private slots:
    void startFetchJobWithNumActivities(const int numActivities = 50);

private:
    int _numRowsPrev = 0;
};

}
