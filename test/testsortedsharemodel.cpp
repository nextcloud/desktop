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

#include "gui/filedetails/sortedsharemodel.h"

#include <QTest>
#include <QAbstractItemModelTester>
#include <QSignalSpy>

#include "sharetestutils.h"

using namespace OCC;

class TestSortedShareModel : public QObject
{
    Q_OBJECT

public slots:
    void addAllTestShares()
    {
        // Let's insert them in the opposite order we want from the model
        for (auto it = _expectedOrder.crbegin(); it != _expectedOrder.crend(); ++it) {
            const auto shareDef = *it;
            if(it->shareType == Share::TypeInternalLink || it->shareType == Share::TypePlaceholderLink) {
                continue; // Don't add the shares that are only internal in the client
            }

            helper.appendShareReplyData(*it);
        }
    }

private:
    ShareTestHelper helper;

    FakeShareDefinition _userADefinition;
    FakeShareDefinition _userBDefinition;
    FakeShareDefinition _groupADefinition;
    FakeShareDefinition _groupBDefinition;
    FakeShareDefinition _linkADefinition;
    FakeShareDefinition _linkBDefinition;
    FakeShareDefinition _emailADefinition;
    FakeShareDefinition _emailBDefinition;
    FakeShareDefinition _remoteADefinition;
    FakeShareDefinition _remoteBDefinition;
    FakeShareDefinition _roomADefinition;
    FakeShareDefinition _roomBDefinition;
    FakeShareDefinition _internalLinkDefinition;

    QVector<FakeShareDefinition> _expectedOrder;

    static constexpr auto _expectedRemoteShareCount = 12;

private slots:
    void initTestCase()
    {
        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);

        QStandardPaths::setTestModeEnabled(true);

        QSignalSpy helperSetupSucceeded(&helper, &ShareTestHelper::setupSucceeded);
        helper.setup();
        QCOMPARE(helperSetupSucceeded.count(), 1);

        const auto userAShareWith = QStringLiteral("user_a");
        const auto userAShareWithDisplayName = QStringLiteral("User A");
        _userADefinition = FakeShareDefinition(&helper, Share::TypeUser, userAShareWith, userAShareWithDisplayName);

        const auto userBShareWith = QStringLiteral("user_b");
        const auto userBShareWithDisplayName = QStringLiteral("User B");
        _userBDefinition = FakeShareDefinition(&helper, Share::TypeUser, userBShareWith, userBShareWithDisplayName);

        const auto groupAShareWith = QStringLiteral("group_a");
        const auto groupAShareWithDisplayName = QStringLiteral("Group A");
        _groupADefinition = FakeShareDefinition(&helper, Share::TypeGroup, groupAShareWith, groupAShareWithDisplayName);

        const auto groupBShareWith = QStringLiteral("group_b");
        const auto groupBShareWithDisplayName = QStringLiteral("Group B");
        _groupBDefinition = FakeShareDefinition(&helper, Share::TypeGroup, groupBShareWith, groupBShareWithDisplayName);

        const auto linkALabel = QStringLiteral("Link share label A");
        _linkADefinition = FakeShareDefinition(&helper, Share::TypeLink, {}, linkALabel);

        const auto linkBLabel = QStringLiteral("Link share label B");
        _linkBDefinition = FakeShareDefinition(&helper, Share::TypeLink, {}, linkBLabel);

        const auto emailAShareWith = QStringLiteral("email_a@nextcloud.com");
        const auto emailAShareWithDisplayName = QStringLiteral("email_a@nextcloud.com");
        _emailADefinition = FakeShareDefinition(&helper, Share::TypeEmail, emailAShareWith, emailAShareWithDisplayName);

        const auto emailBShareWith = QStringLiteral("email_b@nextcloud.com");
        const auto emailBShareWithDisplayName = QStringLiteral("email_b@nextcloud.com");
        _emailBDefinition = FakeShareDefinition(&helper, Share::TypeEmail, emailBShareWith, emailBShareWithDisplayName);

        const auto remoteAShareWith = QStringLiteral("remote_a");
        const auto remoteAShareWithDisplayName = QStringLiteral("Remote share A");
        _remoteADefinition = FakeShareDefinition(&helper, Share::TypeRemote, remoteAShareWith, remoteAShareWithDisplayName);

        const auto remoteBShareWith = QStringLiteral("remote_b");
        const auto remoteBShareWithDisplayName = QStringLiteral("Remote share B");
        _remoteBDefinition = FakeShareDefinition(&helper, Share::TypeRemote, remoteBShareWith, remoteBShareWithDisplayName);

        const auto roomAShareWith = QStringLiteral("room_a");
        const auto roomAShareWithDisplayName = QStringLiteral("Room A");
        _roomADefinition = FakeShareDefinition(&helper, Share::TypeRoom, roomAShareWith, roomAShareWithDisplayName);

        const auto roomBShareWith = QStringLiteral("room_b");
        const auto roomBShareWithDisplayName = QStringLiteral("Room B");
        _roomBDefinition = FakeShareDefinition(&helper, Share::TypeRoom, roomBShareWith, roomBShareWithDisplayName);

        // Dummy internal link share, just use it to check position
        _internalLinkDefinition.shareId = QStringLiteral("__internalLinkShareId__");
        _internalLinkDefinition.shareType = Share::TypeInternalLink;

        _expectedOrder = {// Placeholder link shares always go first, followed by normal link shares.
                          _linkADefinition,
                          _linkBDefinition,
                          // For all other share types, we follow the Share::ShareType enum.
                          _userADefinition,
                          _userBDefinition,
                          _groupADefinition,
                          _groupBDefinition,
                          _emailADefinition,
                          _emailBDefinition,
                          _remoteADefinition,
                          _remoteBDefinition,
                          _roomADefinition,
                          _roomBDefinition,
                          _internalLinkDefinition};
    }

    void testSetModel()
    {
        helper.resetTestData();
        addAllTestShares();
        QCOMPARE(helper.shareCount(), _expectedRemoteShareCount);

        ShareModel model;
        QSignalSpy sharesChanged(&model, &ShareModel::sharesChanged);
        model.setAccountState(helper.accountState.data());
        model.setLocalPath(helper.fakeFolder.localPath() + helper.testFileName);
        QVERIFY(sharesChanged.wait(5000));
        QCOMPARE(model.rowCount(), helper.shareCount() + 1); // Remember the internal link share!

        SortedShareModel sortedModel;
        QAbstractItemModelTester sortedModelTester(&sortedModel);
        QSignalSpy sortedModelReset(&sortedModel, &SortedShareModel::modelReset);
        QSignalSpy shareModelChanged(&sortedModel, &SortedShareModel::sourceModelChanged);

        sortedModel.setSourceModel(&model);
        QCOMPARE(shareModelChanged.count(), 1);
        QCOMPARE(sortedModelReset.count(), 1);
        QCOMPARE(sortedModel.rowCount(), model.rowCount());
        QCOMPARE(sortedModel.sourceModel(), &model);
    }

    void testCorrectSort()
    {
        helper.resetTestData();
        addAllTestShares();
        QCOMPARE(helper.shareCount(), _expectedRemoteShareCount);

        ShareModel model;
        QSignalSpy sharesChanged(&model, &ShareModel::sharesChanged);
        model.setAccountState(helper.accountState.data());
        model.setLocalPath(helper.fakeFolder.localPath() + helper.testFileName);
        QVERIFY(sharesChanged.wait(5000));
        QCOMPARE(model.rowCount(), helper.shareCount() + 1); // Remember the internal link share!

        SortedShareModel sortedModel;
        QAbstractItemModelTester sortedModelTester(&sortedModel);
        QSignalSpy sortedModelReset(&sortedModel, &SortedShareModel::modelReset);

        sortedModel.setSourceModel(&model);
        QCOMPARE(sortedModelReset.count(), 1);
        QCOMPARE(sortedModel.rowCount(), model.rowCount());

        for(auto i = 0; i < sortedModel.rowCount(); ++i) {
            const auto shareIndex = sortedModel.index(i, 0);
            const auto expectedShareDefinition = _expectedOrder.at(i);

            QCOMPARE(shareIndex.data(ShareModel::ShareTypeRole).toInt(), expectedShareDefinition.shareType);
            QCOMPARE(shareIndex.data(ShareModel::ShareIdRole).toString(), expectedShareDefinition.shareId);
        }
    }

};

QTEST_MAIN(TestSortedShareModel)
#include "testsortedsharemodel.moc"
