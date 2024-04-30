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

#include "gui/filedetails/sharemodel.h"

#include <QTest>
#include <QAbstractItemModelTester>
#include <QSignalSpy>
#include <QFileInfo>
#include <QFlags>
#include <QDateTime>
#include <QTimeZone>

#include "sharetestutils.h"
#include "libsync/theme.h"

using namespace OCC;

class TestShareModel : public QObject
{
    Q_OBJECT

private:
    ShareTestHelper helper;

    FakeShareDefinition _testLinkShareDefinition;
    FakeShareDefinition _testEmailShareDefinition;
    FakeShareDefinition _testUserShareDefinition;
    FakeShareDefinition _testRemoteShareDefinition;

private slots:
    void initTestCase()
    {
        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);

        QStandardPaths::setTestModeEnabled(true);

        QSignalSpy helperSetupSucceeded(&helper, &ShareTestHelper::setupSucceeded);
        helper.setup();
        QCOMPARE(helperSetupSucceeded.count(), 1);

        const auto testSharePassword = "3|$argon2id$v=19$m=65536,"
                                       "t=4,"
                                       "p=1$M2FoLnliWkhIZkwzWjFBQg$BPraP+JUqP1sV89rkymXpCGxHBlCct6bZ39xUGaYQ5w";
        const auto testShareNote = QStringLiteral("This is a note!");
        const auto testShareExpiration = QDate::currentDate().addDays(1).toString(helper.expectedDtFormat);

        const auto linkShareLabel = QStringLiteral("Link share label");
        _testLinkShareDefinition = FakeShareDefinition(&helper,
                                                      Share::TypeLink,
                                                      {},
                                                      linkShareLabel,
                                                      testSharePassword,
                                                      testShareNote,
                                                      testShareExpiration);

        const auto emailShareShareWith = QStringLiteral("test-email@nextcloud.com");
        const auto emailShareShareWithDisplayName = QStringLiteral("Test email");
        _testEmailShareDefinition = FakeShareDefinition(&helper,
                                                        Share::TypeEmail,
                                                        emailShareShareWith,
                                                        emailShareShareWithDisplayName,
                                                        testSharePassword,
                                                        testShareNote,
                                                        testShareExpiration);


        const auto userShareShareWith = QStringLiteral("user");
        const auto userShareShareWithDisplayName("A Nextcloud user");
        _testUserShareDefinition = FakeShareDefinition(&helper,
                                                       Share::TypeUser,
                                                       userShareShareWith,
                                                       userShareShareWithDisplayName);



        const auto remoteShareShareWith = QStringLiteral("remote_share");
        const auto remoteShareShareWithDisplayName("A remote share");
        _testRemoteShareDefinition = FakeShareDefinition(&helper,
                                                         Share::TypeRemote,
                                                         remoteShareShareWith,
                                                         remoteShareShareWithDisplayName);

        qRegisterMetaType<ShareePtr>("ShareePtr");
    }

    void testSetAccountAndPath()
    {
        helper.resetTestData();
        // Test with a link share
        helper.appendShareReplyData(_testLinkShareDefinition);
        QCOMPARE(helper.shareCount(), 1);

        ShareModel model;
        QAbstractItemModelTester modelTester(&model);
        QCOMPARE(model.rowCount(), 0);

        QSignalSpy accountStateChanged(&model, &ShareModel::accountStateChanged);
        QSignalSpy localPathChanged(&model, &ShareModel::localPathChanged);

        QSignalSpy accountConnectedChanged(&model, &ShareModel::accountConnectedChanged);
        QSignalSpy sharingEnabledChanged(&model, &ShareModel::sharingEnabledChanged);
        QSignalSpy publicLinkSharesEnabledChanged(&model, &ShareModel::publicLinkSharesEnabledChanged);

        model.setAccountState(helper.accountState.data());
        QCOMPARE(accountStateChanged.count(), 1);

        // Check all the account-related properties of the model
        QCOMPARE(model.accountConnected(), helper.accountState->isConnected());
        QCOMPARE(model.sharingEnabled(), helper.account->capabilities().shareAPI());
        QCOMPARE(model.publicLinkSharesEnabled() && Theme::instance()->linkSharing(), helper.account->capabilities().sharePublicLink());
        QCOMPARE(Theme::instance()->userGroupSharing(), model.userGroupSharingEnabled());

        const QString localPath(helper.fakeFolder.localPath() + helper.testFileName);
        model.setLocalPath(localPath);
        QCOMPARE(localPathChanged.count(), 1);
        QCOMPARE(model.localPath(), localPath);
    }

    void testSuccessfulFetchShares()
    {
        helper.resetTestData();
        // Test with a link share and a user/group email share "from the server"
        helper.appendShareReplyData(_testLinkShareDefinition);
        helper.appendShareReplyData(_testEmailShareDefinition);
        helper.appendShareReplyData(_testUserShareDefinition);
        QCOMPARE(helper.shareCount(), 3);

        ShareModel model;
        QAbstractItemModelTester modelTester(&model);
        QCOMPARE(model.rowCount(), 0);

        QSignalSpy sharesChanged(&model, &ShareModel::sharesChanged);

        model.setAccountState(helper.accountState.data());
        model.setLocalPath(helper.fakeFolder.localPath() + helper.testFileName);

        QVERIFY(sharesChanged.wait(5000));
        QCOMPARE(model.rowCount(), helper.shareCount() + 1); // Internal link share
    }

    void testFetchSharesFailedError()
    {
        helper.resetTestData();
        // Test with a link share "from the server"
        helper.appendShareReplyData(_testLinkShareDefinition);

        ShareModel model;
        QAbstractItemModelTester modelTester(&model);
        QCOMPARE(model.rowCount(), 0);

        QSignalSpy serverError(&model, &ShareModel::serverError);

        // Test fetching the shares of a file that does not exist
        model.setAccountState(helper.accountState.data());
        model.setLocalPath(helper.fakeFolder.localPath() + "wrong-filename-oops.md");
        QVERIFY(serverError.wait(3000));
        QCOMPARE(model.hasInitialShareFetchCompleted(), true);
        QCOMPARE(model.rowCount(), 0); // Make sure no placeholder nor internal link share
    }

    void testCorrectFetchOngoingSignalling()
    {
        helper.resetTestData();

        // Test with a link share "from the server"
        helper.appendShareReplyData(_testLinkShareDefinition);
        QCOMPARE(helper.shareCount(), 1);

        ShareModel model;
        QAbstractItemModelTester modelTester(&model);
        QCOMPARE(model.rowCount(), 0);

        QSignalSpy fetchOngoingChanged(&model, &ShareModel::fetchOngoingChanged);

        // Make sure we are correctly signalling the loading state of the fetch
        // Model resets twice when we set account and local path, resetting all model state.

        model.setAccountState(helper.accountState.data());
        QCOMPARE(fetchOngoingChanged.count(), 1);
        QCOMPARE(model.fetchOngoing(), false);

        model.setLocalPath(helper.fakeFolder.localPath() + helper.testFileName);
        // If we can grab shares it then indicates fetch ongoing...
        QCOMPARE(fetchOngoingChanged.count(), 3);
        QCOMPARE(model.fetchOngoing(), true);

        // Then indicates fetch finished when done.
        QVERIFY(fetchOngoingChanged.wait(3000));
        QCOMPARE(model.fetchOngoing(), false);
    }

    void testCorrectInitialFetchCompleteSignalling()
    {
        helper.resetTestData();

        // Test with a link share "from the server"
        helper.appendShareReplyData(_testLinkShareDefinition);
        QCOMPARE(helper.shareCount(), 1);

        ShareModel model;
        QAbstractItemModelTester modelTester(&model);
        QCOMPARE(model.rowCount(), 0);

        QSignalSpy accountStateChanged(&model, &ShareModel::accountStateChanged);
        QSignalSpy localPathChanged(&model, &ShareModel::localPathChanged);
        QSignalSpy hasInitialShareFetchCompletedChanged(&model, &ShareModel::hasInitialShareFetchCompletedChanged);

        // Make sure we are correctly signalling the loading state of the fetch
        // Model resets twice when we set account and local path, resetting all model state.

        model.setAccountState(helper.accountState.data());
        QCOMPARE(accountStateChanged.count(), 1);
        QCOMPARE(hasInitialShareFetchCompletedChanged.count(), 1);
        QCOMPARE(model.hasInitialShareFetchCompleted(), false);

        model.setLocalPath(helper.fakeFolder.localPath() + helper.testFileName);
        QCOMPARE(localPathChanged.count(), 1);
        QCOMPARE(hasInitialShareFetchCompletedChanged.count(), 2);
        QCOMPARE(model.hasInitialShareFetchCompleted(), false);

        // Once we have acquired shares from the server the initial share fetch is completed
        QVERIFY(hasInitialShareFetchCompletedChanged.wait(3000));
        QCOMPARE(hasInitialShareFetchCompletedChanged.count(), 3);
        QCOMPARE(model.hasInitialShareFetchCompleted(), true);
    }

    // Link shares and user group shares have slightly different behaviour in model.data()
    void testModelLinkShareData()
    {
        helper.resetTestData();
        // Test with a link share "from the server"
        helper.appendShareReplyData(_testLinkShareDefinition);
        QCOMPARE(helper.shareCount(), 1);

        ShareModel model;
        QAbstractItemModelTester modelTester(&model);
        QCOMPARE(model.rowCount(), 0);

        QSignalSpy sharesChanged(&model, &ShareModel::sharesChanged);

        model.setAccountState(helper.accountState.data());
        model.setLocalPath(helper.fakeFolder.localPath() + helper.testFileName);

        QVERIFY(sharesChanged.wait(5000));
        QCOMPARE(model.rowCount(), helper.shareCount() + 1); // Remember internal link share!

        // Placeholder link share gets added after we are done parsing fetched shares, and the
        // internal link share is added after we receive a reply from the PROPFIND, which we
        // send before fetching the shares, so it will be added first.
        //
        // Hence we grab the remote share in between.
        const auto shareIndex = model.index(model.rowCount() - 1, 0, {});
        QVERIFY(!shareIndex.data(Qt::DisplayRole).toString().isEmpty());
        QCOMPARE(shareIndex.data(ShareModel::ShareTypeRole).toInt(), _testLinkShareDefinition.shareType);
        QCOMPARE(shareIndex.data(ShareModel::ShareIdRole).toString(), _testLinkShareDefinition.shareId);
        QCOMPARE(shareIndex.data(ShareModel::LinkRole).toString(), _testLinkShareDefinition.linkShareUrl);
        QCOMPARE(shareIndex.data(ShareModel::LinkShareNameRole).toString(), _testLinkShareDefinition.linkShareName);
        QCOMPARE(shareIndex.data(ShareModel::LinkShareLabelRole).toString(), _testLinkShareDefinition.linkShareLabel);
        QCOMPARE(shareIndex.data(ShareModel::NoteEnabledRole).toBool(), !_testLinkShareDefinition.shareNote.isEmpty());
        QCOMPARE(shareIndex.data(ShareModel::NoteRole).toString(), _testLinkShareDefinition.shareNote);
        QCOMPARE(shareIndex.data(ShareModel::PasswordProtectEnabledRole).toBool(), !_testLinkShareDefinition.sharePassword.isEmpty());
        // We don't expose the fetched password to the user as it's useless to them
        QCOMPARE(shareIndex.data(ShareModel::PasswordRole).toString(), QString());
        QCOMPARE(shareIndex.data(ShareModel::EditingAllowedRole).toBool(), SharePermissions(_testLinkShareDefinition.sharePermissions).testFlag(SharePermissionUpdate));

        const auto expectedLinkShareExpireDate = QDate::fromString(_testLinkShareDefinition.shareExpiration, helper.expectedDtFormat);
        QCOMPARE(shareIndex.data(ShareModel::ExpireDateEnabledRole).toBool(), expectedLinkShareExpireDate.isValid());
        QCOMPARE(shareIndex.data(ShareModel::ExpireDateRole).toLongLong(), expectedLinkShareExpireDate.startOfDay(Qt::UTC).toMSecsSinceEpoch());

        const auto iconUrl = shareIndex.data(ShareModel::IconUrlRole).toString();
        QVERIFY(iconUrl.contains("public.svg"));
    }

    void testModelEmailShareData()
    {
        helper.resetTestData();
        // Test with a user/group email share "from the server"
        helper.appendShareReplyData(_testEmailShareDefinition);
        QCOMPARE(helper.shareCount(), 1);

        ShareModel model;
        QAbstractItemModelTester modelTester(&model);
        QCOMPARE(model.rowCount(), 0);

        QSignalSpy sharesChanged(&model, &ShareModel::sharesChanged);

        model.setAccountState(helper.accountState.data());
        model.setLocalPath(helper.fakeFolder.localPath() + helper.testFileName);

        QVERIFY(sharesChanged.wait(5000));
        QCOMPARE(model.rowCount(), 3); // Remember about placeholder and internal link share

        // Placeholder link share gets added after we are done parsing fetched shares, and the
        // internal link share is added after we receive a reply from the PROPFIND, which we
        // send before fetching the shares, so it will be added first.
        //
        // Hence we grab the remote share in between.
        const auto shareIndex = model.index(1, 0, {});
        QVERIFY(!shareIndex.data(Qt::DisplayRole).toString().isEmpty());
        QCOMPARE(shareIndex.data(ShareModel::ShareTypeRole).toInt(), _testEmailShareDefinition.shareType);
        QCOMPARE(shareIndex.data(ShareModel::ShareIdRole).toString(), _testEmailShareDefinition.shareId);
        QCOMPARE(shareIndex.data(ShareModel::NoteEnabledRole).toBool(), !_testEmailShareDefinition.shareNote.isEmpty());
        QCOMPARE(shareIndex.data(ShareModel::NoteRole).toString(), _testEmailShareDefinition.shareNote);
        QCOMPARE(shareIndex.data(ShareModel::PasswordProtectEnabledRole).toBool(), !_testEmailShareDefinition.sharePassword.isEmpty());
        // We don't expose the fetched password to the user as it's useless to them
        QCOMPARE(shareIndex.data(ShareModel::PasswordRole).toString(), QString());
        QCOMPARE(shareIndex.data(ShareModel::EditingAllowedRole).toBool(), SharePermissions(_testEmailShareDefinition.sharePermissions).testFlag(SharePermissionUpdate));

        const auto expectedShareExpireDate = QDate::fromString(_testEmailShareDefinition.shareExpiration, helper.expectedDtFormat);
        QCOMPARE(shareIndex.data(ShareModel::ExpireDateEnabledRole).toBool(), expectedShareExpireDate.isValid());
        QCOMPARE(shareIndex.data(ShareModel::ExpireDateRole).toLongLong(), expectedShareExpireDate.startOfDay(Qt::UTC).toMSecsSinceEpoch());

        const auto iconUrl = shareIndex.data(ShareModel::IconUrlRole).toString();
        QVERIFY(iconUrl.contains("email.svg"));
    }

    void testModelUserShareData()
    {
        helper.resetTestData();
        // Test with a user/group user share "from the server"
        helper.appendShareReplyData(_testUserShareDefinition);
        QCOMPARE(helper.shareCount(), 1);

        ShareModel model;
        QAbstractItemModelTester modelTester(&model);
        QCOMPARE(model.rowCount(), 0);

        QSignalSpy sharesChanged(&model, &ShareModel::sharesChanged);

        model.setAccountState(helper.accountState.data());
        model.setLocalPath(helper.fakeFolder.localPath() + helper.testFileName);

        QVERIFY(sharesChanged.wait(5000));
        QCOMPARE(model.rowCount(), 3); // Remember about placeholder and internal link share

        // Placeholder link share gets added after we are done parsing fetched shares, and the
        // internal link share is added after we receive a reply from the PROPFIND, which we
        // send before fetching the shares, so it will be added first.
        //
        // Hence we grab the remote share in between.
        const auto shareIndex = model.index(1, 0, {});
        QVERIFY(!shareIndex.data(Qt::DisplayRole).toString().isEmpty());
        QCOMPARE(shareIndex.data(ShareModel::ShareTypeRole).toInt(), _testUserShareDefinition.shareType);
        QCOMPARE(shareIndex.data(ShareModel::ShareIdRole).toString(), _testUserShareDefinition.shareId);
        QCOMPARE(shareIndex.data(ShareModel::NoteEnabledRole).toBool(), !_testUserShareDefinition.shareNote.isEmpty());
        QCOMPARE(shareIndex.data(ShareModel::NoteRole).toString(), _testUserShareDefinition.shareNote);
        QCOMPARE(shareIndex.data(ShareModel::PasswordProtectEnabledRole).toBool(), !_testUserShareDefinition.sharePassword.isEmpty());
        // We don't expose the fetched password to the user as it's useless to them
        QCOMPARE(shareIndex.data(ShareModel::PasswordRole).toString(), QString());
        QCOMPARE(shareIndex.data(ShareModel::EditingAllowedRole).toBool(), SharePermissions(_testUserShareDefinition.sharePermissions).testFlag(SharePermissionUpdate));

        const auto expectedShareExpireDate = QDate::fromString(_testUserShareDefinition.shareExpiration, helper.expectedDtFormat);
        QCOMPARE(shareIndex.data(ShareModel::ExpireDateEnabledRole).toBool(), expectedShareExpireDate.isValid());
        QCOMPARE(shareIndex.data(ShareModel::ExpireDateRole).toLongLong(), expectedShareExpireDate.startOfDay(Qt::UTC).toMSecsSinceEpoch());

        const auto iconUrl = shareIndex.data(ShareModel::IconUrlRole).toString();
        QVERIFY(iconUrl.contains("user.svg"));

        // Check correct user avatar
        const auto avatarUrl = shareIndex.data(ShareModel::AvatarUrlRole).toString();
        const auto relativeAvatarPath = QString("remote.php/dav/avatars/%1/%2.png").arg(_testUserShareDefinition.shareShareWith, QString::number(64));
        const auto expectedAvatarPath = Utility::concatUrlPath(helper.account->url(), relativeAvatarPath).toString();
        const QString expectedUrl(QStringLiteral("image://tray-image-provider/") + expectedAvatarPath);
        QCOMPARE(avatarUrl, expectedUrl);
    }

    void testSuccessfulCreateShares()
    {
        helper.resetTestData();

        // Test with an existing link share
        helper.appendShareReplyData(_testLinkShareDefinition);
        QCOMPARE(helper.shareCount(), 1);

        ShareModel model;
        QAbstractItemModelTester modelTester(&model);
        QCOMPARE(model.rowCount(), 0);

        QSignalSpy sharesChanged(&model, &ShareModel::sharesChanged);

        model.setAccountState(helper.accountState.data());
        model.setLocalPath(helper.fakeFolder.localPath() + helper.testFileName);

        QVERIFY(sharesChanged.wait(5000));
        QCOMPARE(helper.shareCount(), 1); // Check our test is working!
        QCOMPARE(model.rowCount(), helper.shareCount() + 1); // Internal link share!

        // Test if it gets added
        model.createNewLinkShare();
        QVERIFY(sharesChanged.wait(5000));
        QCOMPARE(helper.shareCount(), 2); // Check our test is working!
        QCOMPARE(model.rowCount(), helper.shareCount() + 1); // Internal link share!

        // Test if it's the type we wanted
        const auto newLinkShareIndex = model.index(model.rowCount() - 1, 0, {});
        QCOMPARE(newLinkShareIndex.data(ShareModel::ShareTypeRole).toInt(), Share::TypeLink);

        // Do it again with a different type
        const ShareePtr sharee(new Sharee("testsharee@nextcloud.com", "Test sharee", Sharee::Type::Email));
        model.createNewUserGroupShare(sharee);
        QVERIFY(sharesChanged.wait(5000));
        QCOMPARE(helper.shareCount(), 3); // Check our test is working!
        QCOMPARE(model.rowCount(), helper.shareCount() + 1); // Internal link share!

        // Test if it's the type we wanted
        const auto newUserGroupShareIndex = model.index(model.rowCount() - 1, 0, {});
        QCOMPARE(newUserGroupShareIndex.data(ShareModel::ShareTypeRole).toInt(), Share::TypeEmail);

        // Confirm correct addition of share with password
        const auto password = QStringLiteral("a pretty bad password but good thing it doesn't matter!");
        model.createNewLinkShareWithPassword(password);
        QVERIFY(sharesChanged.wait(5000));
        QCOMPARE(helper.shareCount(), 4); // Check our test is working!
        QCOMPARE(model.rowCount(), helper.shareCount() + 1); // Internal link share!

        model.createNewUserGroupShareWithPassword(sharee, password);
        QVERIFY(sharesChanged.wait(5000));
        QCOMPARE(helper.shareCount(), 5); // Check our test is working!
        QCOMPARE(model.rowCount(), helper.shareCount() + 1); // Internal link share!

        helper.resetTestData();
    }

    void testEnforcePasswordShares()
    {
        helper.resetTestData();

        // Enforce passwords for shares in capabilities
        const QVariantMap enforcePasswordsCapabilities {
            {QStringLiteral("files_sharing"), QVariantMap {
                {QStringLiteral("api_enabled"), true},
                {QStringLiteral("default_permissions"), 19},
                {QStringLiteral("public"), QVariantMap {
                    {QStringLiteral("enabled"), true},
                    {QStringLiteral("expire_date"), QVariantMap {
                        {QStringLiteral("days"), 30},
                        {QStringLiteral("enforced"), false},
                    }},
                    {QStringLiteral("expire_date_internal"), QVariantMap {
                         {QStringLiteral("days"), 30},
                         {QStringLiteral("enforced"), false},
                    }},
                    {QStringLiteral("expire_date_remote"), QVariantMap {
                         {QStringLiteral("days"), 30},
                         {QStringLiteral("enforced"), false},
                    }},
                    {QStringLiteral("password"), QVariantMap {
                        {QStringLiteral("enforced"), true},
                    }},
                }},
                {QStringLiteral("sharebymail"), QVariantMap {
                    {QStringLiteral("enabled"), true},
                    {QStringLiteral("password"), QVariantMap {
                        {QStringLiteral("enforced"), true},
                    }},
                }},
            }},
        };

        helper.account->setCapabilities(enforcePasswordsCapabilities);
        QVERIFY(helper.account->capabilities().sharePublicLinkEnforcePassword());
        QVERIFY(helper.account->capabilities().shareEmailPasswordEnforced());

        // Test with a link share "from the server"
        helper.appendShareReplyData(_testLinkShareDefinition);
        QCOMPARE(helper.shareCount(), 1);

        ShareModel model;
        QAbstractItemModelTester modelTester(&model);
        QCOMPARE(model.rowCount(), 0);

        QSignalSpy sharesChanged(&model, &ShareModel::sharesChanged);

        model.setAccountState(helper.accountState.data());
        model.setLocalPath(helper.fakeFolder.localPath() + helper.testFileName);

        QVERIFY(sharesChanged.wait(5000));
        QCOMPARE(model.rowCount(), helper.shareCount() + 1); // Internal link share!

        // Confirm that the model requests a password
        QSignalSpy requestPasswordForLinkShare(&model, &ShareModel::requestPasswordForLinkShare);
        model.createNewLinkShare();
        QVERIFY(requestPasswordForLinkShare.wait(3000));

        QSignalSpy requestPasswordForEmailShare(&model, &ShareModel::requestPasswordForEmailSharee);
        const ShareePtr sharee(new Sharee("testsharee@nextcloud.com", "Test sharee", Sharee::Type::Email));
        model.createNewUserGroupShare(sharee);
        QCOMPARE(requestPasswordForEmailShare.count(), 1);

        // Test that the model data is correctly reporting that passwords are enforced
        const auto shareIndex = model.index(model.rowCount() - 1, 0, {});
        QCOMPARE(shareIndex.data(ShareModel::PasswordEnforcedRole).toBool(), true);
        QCOMPARE(shareIndex.data(ShareModel::PasswordProtectEnabledRole).toBool(), true);
    }

    void testEnforceExpireDate()
    {
        helper.resetTestData();

        const auto internalExpireDays = 45;
        const auto publicExpireDays = 30;
        const auto remoteExpireDays = 25;

        // Enforce expire dates for shares in capabilities
        const QVariantMap enforcePasswordsCapabilities {
            {QStringLiteral("files_sharing"), QVariantMap {
                {QStringLiteral("api_enabled"), true},
                {QStringLiteral("default_permissions"), 19},
                {QStringLiteral("public"), QVariantMap {
                    {QStringLiteral("enabled"), true},
                    {QStringLiteral("expire_date"), QVariantMap {
                         {QStringLiteral("days"), publicExpireDays},
                         {QStringLiteral("enforced"), true},
                    }},
                    {QStringLiteral("expire_date_internal"), QVariantMap {
                         {QStringLiteral("days"), internalExpireDays},
                         {QStringLiteral("enforced"), true},
                    }},
                    {QStringLiteral("expire_date_remote"), QVariantMap {
                         {QStringLiteral("days"), remoteExpireDays},
                         {QStringLiteral("enforced"), true},
                    }},
                    {QStringLiteral("password"), QVariantMap {
                        {QStringLiteral("enforced"), false},
                    }},
                 }},
                 {QStringLiteral("sharebymail"), QVariantMap {
                     {QStringLiteral("enabled"), true},
                     {QStringLiteral("password"), QVariantMap {
                         {QStringLiteral("enforced"), true},
                     }},
                 }},
            }},
        };

        helper.account->setCapabilities(enforcePasswordsCapabilities);
        QVERIFY(helper.account->capabilities().sharePublicLinkEnforceExpireDate());
        QVERIFY(helper.account->capabilities().shareInternalEnforceExpireDate());
        QVERIFY(helper.account->capabilities().shareRemoteEnforceExpireDate());

        // Test with shares "from the server"
        helper.appendShareReplyData(_testLinkShareDefinition);
        helper.appendShareReplyData(_testEmailShareDefinition);
        helper.appendShareReplyData(_testRemoteShareDefinition);
        QCOMPARE(helper.shareCount(), 3);

        ShareModel model;
        QAbstractItemModelTester modelTester(&model);
        QCOMPARE(model.rowCount(), 0);

        QSignalSpy sharesChanged(&model, &ShareModel::sharesChanged);

        model.setAccountState(helper.accountState.data());
        model.setLocalPath(helper.fakeFolder.localPath() + helper.testFileName);

        QVERIFY(sharesChanged.wait(5000));
        QCOMPARE(model.rowCount(), helper.shareCount() + 1); // Internal link share!

        // Test that the model data is correctly reporting that expire dates are enforced for all share types
        for(auto i = 0; i < model.rowCount(); ++i) {
            const auto shareIndex = model.index(i, 0, {});
            const auto shareType = shareIndex.data(ShareModel::ShareTypeRole).toInt();
            const auto expectTrue = shareType != ShareModel::ShareTypePlaceholderLink &&
                                    shareType != ShareModel::ShareTypeInternalLink;
            QCOMPARE(shareIndex.data(ShareModel::ExpireDateEnforcedRole).toBool(), expectTrue);

            QDateTime expectedExpireDateTime;
            switch(shareType) {
            case Share::TypeInternalLink:
            case Share::TypePlaceholderLink:
                return;
            case Share::TypeUser:
            case Share::TypeGroup:
            case Share::TypeCircle:
            case Share::TypeRoom:
                expectedExpireDateTime = QDate::currentDate().addDays(internalExpireDays).startOfDay(QTimeZone::utc());
                break;
            case Share::TypeLink:
            case Share::TypeEmail:
                expectedExpireDateTime = QDate::currentDate().addDays(publicExpireDays).startOfDay(QTimeZone::utc());
                break;
            case Share::TypeRemote:
                expectedExpireDateTime = QDate::currentDate().addDays(remoteExpireDays).startOfDay(QTimeZone::utc());
                break;
            }

            QCOMPARE(shareIndex.data(ShareModel::EnforcedMaximumExpireDateRole).toLongLong(), expectedExpireDateTime.toMSecsSinceEpoch());
        }
    }

    void testSuccessfulDeleteShares()
    {
        helper.resetTestData();

        // Test with an existing link share
        helper.appendShareReplyData(_testLinkShareDefinition);
        QCOMPARE(helper.shareCount(), 1);

        ShareModel model;
        QAbstractItemModelTester modelTester(&model);
        QCOMPARE(model.rowCount(), 0);

        QSignalSpy sharesChanged(&model, &ShareModel::sharesChanged);

        model.setAccountState(helper.accountState.data());
        model.setLocalPath(helper.fakeFolder.localPath() + helper.testFileName);

        QVERIFY(sharesChanged.wait(5000));
        QCOMPARE(helper.shareCount(), 1); // Check our test is working!
        QCOMPARE(model.rowCount(), helper.shareCount() + 1); // Internal link share!

        // Create share
        model.createNewLinkShare();
        QVERIFY(sharesChanged.wait(5000));
        QCOMPARE(helper.shareCount(), 2); // Check our test is working!
        QCOMPARE(model.rowCount(), helper.shareCount() + 1); // Internal link share!

        // Test if it gets deleted properly
        const auto latestLinkShare = model.index(model.rowCount() - 1, 0, {}).data(ShareModel::ShareRole).value<SharePtr>();
        QSignalSpy shareDeleted(latestLinkShare.data(), &LinkShare::shareDeleted);
        model.deleteShare(latestLinkShare);
        QVERIFY(shareDeleted.wait(5000));
        QCOMPARE(helper.shareCount(), 1); // Check our test is working!
        QCOMPARE(model.rowCount(), helper.shareCount() + 1); // Internal link share!

        helper.resetTestData();
    }

    void testPlaceholderLinkShare()
    {
        helper.resetTestData();

        // Start with no shares; should show the placeholder link share
        ShareModel model;
        QAbstractItemModelTester modelTester(&model);
        QCOMPARE(model.rowCount(), 0); // There should be no placeholder yet

        QSignalSpy hasInitialShareFetchCompletedChanged(&model, &ShareModel::hasInitialShareFetchCompletedChanged);

        model.setAccountState(helper.accountState.data());
        model.setLocalPath(helper.fakeFolder.localPath() + helper.testFileName);
        QVERIFY(hasInitialShareFetchCompletedChanged.wait(5000));
        QVERIFY(model.hasInitialShareFetchCompleted());
        QCOMPARE(model.rowCount(), 2); // There should be a placeholder and internal link share now

        const QPersistentModelIndex placeholderLinkShareIndex(model.index(model.rowCount() - 1, 0, {}));
        QCOMPARE(placeholderLinkShareIndex.data(ShareModel::ShareTypeRole).toInt(), Share::TypePlaceholderLink);

        // Test adding a user group share -- we should still be showing a placeholder link share
        QSignalSpy sharesChanged(&model, &ShareModel::sharesChanged);
        const ShareePtr sharee(new Sharee("testsharee@nextcloud.com", "Test sharee", Sharee::Type::Email));
        model.createNewUserGroupShare(sharee);
        QVERIFY(sharesChanged.wait(5000));
        QCOMPARE(helper.shareCount(), 1); // Check our test is working!
        QCOMPARE(model.rowCount(), helper.shareCount() + 2); // Internal link share too!

        QVERIFY(placeholderLinkShareIndex.isValid());
        QCOMPARE(placeholderLinkShareIndex.data(ShareModel::ShareTypeRole).toInt(), Share::TypePlaceholderLink);

        // Now try adding a link share, which should remove the placeholder
        model.createNewLinkShare();
        QVERIFY(sharesChanged.wait(5000));
        QCOMPARE(helper.shareCount(), 2); // Check our test is working!
        QCOMPARE(model.rowCount(), helper.shareCount() + 1); // Internal link share!

        QVERIFY(!placeholderLinkShareIndex.isValid());

        // Now delete the only link share, which should bring back the placeholder link share
        const auto latestLinkShare = model.index(model.rowCount() - 1, 0, {}).data(ShareModel::ShareRole).value<SharePtr>();
        QSignalSpy shareDeleted(latestLinkShare.data(), &LinkShare::shareDeleted);
        model.deleteShare(latestLinkShare);
        QVERIFY(shareDeleted.wait(5000));
        QCOMPARE(helper.shareCount(), 1); // Check our test is working!
        QCOMPARE(model.rowCount(), helper.shareCount() + 2); // Internal link share too!

        const auto newPlaceholderLinkShareIndex = model.index(model.rowCount() - 1, 0, {});
        QCOMPARE(newPlaceholderLinkShareIndex.data(ShareModel::ShareTypeRole).toInt(), Share::TypePlaceholderLink);

        helper.resetTestData();
    }

    void testSuccessfulToggleAllowEditing()
    {
        helper.resetTestData();

        // Test with an existing link share
        helper.appendShareReplyData(_testLinkShareDefinition);
        QCOMPARE(helper.shareCount(), 1);

        ShareModel model;
        QAbstractItemModelTester modelTester(&model);
        QCOMPARE(model.rowCount(), 0);

        QSignalSpy sharesChanged(&model, &ShareModel::sharesChanged);

        model.setAccountState(helper.accountState.data());
        model.setLocalPath(helper.fakeFolder.localPath() + helper.testFileName);

        QVERIFY(sharesChanged.wait(5000));
        QCOMPARE(helper.shareCount(), 1); // Check our test is working!
        QCOMPARE(model.rowCount(), helper.shareCount() + 1); // Internal link share!

        const auto shareIndex = model.index(model.rowCount() - 1, 0, {});
        QCOMPARE(shareIndex.data(ShareModel::EditingAllowedRole).toBool(), SharePermissions(_testLinkShareDefinition.sharePermissions).testFlag(SharePermissionUpdate));

        const auto share = shareIndex.data(ShareModel::ShareRole).value<SharePtr>();
        QSignalSpy permissionsSet(share.data(), &Share::permissionsSet);

        model.toggleShareAllowEditing(share, false);
        QVERIFY(permissionsSet.wait(3000));
        QCOMPARE(shareIndex.data(ShareModel::EditingAllowedRole).toBool(), false);
    }

    void testSuccessfulPasswordSet()
    {
        helper.resetTestData();

        // Test with an existing link share.
        // This one has a pre-existing password
        helper.appendShareReplyData(_testLinkShareDefinition);
        QCOMPARE(helper.shareCount(), 1);

        ShareModel model;
        QAbstractItemModelTester modelTester(&model);
        QCOMPARE(model.rowCount(), 0);

        QSignalSpy sharesChanged(&model, &ShareModel::sharesChanged);

        model.setAccountState(helper.accountState.data());
        model.setLocalPath(helper.fakeFolder.localPath() + helper.testFileName);

        QVERIFY(sharesChanged.wait(5000));
        QCOMPARE(helper.shareCount(), 1); // Check our test is working!
        QCOMPARE(model.rowCount(), helper.shareCount() + 1); // Internal link share!

        const auto shareIndex = model.index(model.rowCount() - 1, 0, {});
        QCOMPARE(shareIndex.data(ShareModel::PasswordProtectEnabledRole).toBool(), true);

        const auto share = shareIndex.data(ShareModel::ShareRole).value<SharePtr>();
        QSignalSpy passwordSet(share.data(), &Share::passwordSet);

        model.toggleSharePasswordProtect(share, false);
        QVERIFY(passwordSet.wait(3000));
        QCOMPARE(shareIndex.data(ShareModel::PasswordProtectEnabledRole).toBool(), false);

        const auto password = QStringLiteral("a pretty bad password but good thing it doesn't matter!");
        model.setSharePassword(share, password);
        QVERIFY(passwordSet.wait(3000));
        QCOMPARE(shareIndex.data(ShareModel::PasswordProtectEnabledRole).toBool(), true);
        // The model stores the recently set password.
        // We want to present the user with it in the UI while the model is alive
        QCOMPARE(shareIndex.data(ShareModel::PasswordRole).toString(), password);
    }

    void testSuccessfulExpireDateSet()
    {
        helper.resetTestData();

        // Test with an existing link share.
        // This one has a pre-existing expire date
        helper.appendShareReplyData(_testLinkShareDefinition);
        QCOMPARE(helper.shareCount(), 1);

        ShareModel model;
        QAbstractItemModelTester modelTester(&model);
        QCOMPARE(model.rowCount(), 0);

        QSignalSpy sharesChanged(&model, &ShareModel::sharesChanged);

        model.setAccountState(helper.accountState.data());
        model.setLocalPath(helper.fakeFolder.localPath() + helper.testFileName);

        QVERIFY(sharesChanged.wait(5000));
        QCOMPARE(helper.shareCount(), 1); // Check our test is working!
        QCOMPARE(model.rowCount(), helper.shareCount() + 1); // Internal link share!

        // Check what we know
        const auto shareIndex = model.index(model.rowCount() - 1, 0, {});
        QCOMPARE(shareIndex.data(ShareModel::ExpireDateEnabledRole).toBool(), true);

        // Disable expire date
        const auto sharePtr = shareIndex.data(ShareModel::ShareRole).value<SharePtr>();
        const auto linkSharePtr = sharePtr.dynamicCast<LinkShare>(); // Need to connect to signal
        QSignalSpy expireDateSet(linkSharePtr.data(), &LinkShare::expireDateSet);
        model.toggleShareExpirationDate(sharePtr, false);

        QVERIFY(expireDateSet.wait(3000));
        QCOMPARE(shareIndex.data(ShareModel::ExpireDateEnabledRole).toBool(), false);

        // Set a new expire date
        const auto expireDateMsecs = QDate::currentDate().addDays(10).startOfDay(Qt::UTC).toMSecsSinceEpoch();
        model.setShareExpireDate(linkSharePtr, expireDateMsecs);
        QVERIFY(expireDateSet.wait(3000));
        QCOMPARE(shareIndex.data(ShareModel::ExpireDateRole).toLongLong(), expireDateMsecs);
        QCOMPARE(shareIndex.data(ShareModel::ExpireDateEnabledRole).toBool(), true);

        // Test the QML-specific slot
        const QVariant newExpireDateMsecs = QDate::currentDate().addDays(20).startOfDay(Qt::UTC).toMSecsSinceEpoch();
        model.setShareExpireDateFromQml(QVariant::fromValue(sharePtr), newExpireDateMsecs);
        QVERIFY(expireDateSet.wait(3000));
        QCOMPARE(shareIndex.data(ShareModel::ExpireDateRole).toLongLong(), newExpireDateMsecs);
        QCOMPARE(shareIndex.data(ShareModel::ExpireDateEnabledRole).toBool(), true);
    }

    void testSuccessfulNoteSet()
    {
        helper.resetTestData();

        // Test with an existing link share.
        // This one has a pre-existing password
        helper.appendShareReplyData(_testLinkShareDefinition);
        QCOMPARE(helper.shareCount(), 1);

        ShareModel model;
        QAbstractItemModelTester modelTester(&model);
        QCOMPARE(model.rowCount(), 0);

        QSignalSpy sharesChanged(&model, &ShareModel::sharesChanged);

        model.setAccountState(helper.accountState.data());
        model.setLocalPath(helper.fakeFolder.localPath() + helper.testFileName);

        QVERIFY(sharesChanged.wait(5000));
        QCOMPARE(helper.shareCount(), 1); // Check our test is working!
        QCOMPARE(model.rowCount(), helper.shareCount() + 1); // Internal link share!

        const auto shareIndex = model.index(model.rowCount() - 1, 0, {});
        QCOMPARE(shareIndex.data(ShareModel::NoteEnabledRole).toBool(), true);

        const auto sharePtr = shareIndex.data(ShareModel::ShareRole).value<SharePtr>();
        const auto linkSharePtr = sharePtr.dynamicCast<LinkShare>(); // Need to connect to signal
        QSignalSpy noteSet(linkSharePtr.data(), &LinkShare::noteSet);

        model.toggleShareNoteToRecipient(sharePtr, false);
        QVERIFY(noteSet.wait(3000));
        QCOMPARE(shareIndex.data(ShareModel::NoteEnabledRole).toBool(), false);

        const auto note = QStringLiteral("Don't forget to test everything!");
        model.setShareNote(sharePtr, note);
        QVERIFY(noteSet.wait(3000));
        QCOMPARE(shareIndex.data(ShareModel::NoteEnabledRole).toBool(), true);
        // The model stores the recently set password.
        // We want to present the user with it in the UI while the model is alive
        QCOMPARE(shareIndex.data(ShareModel::NoteRole).toString(), note);
    }

    void testSuccessfulLinkShareLabelSet()
    {
        helper.resetTestData();

        // Test with an existing link share.
        helper.appendShareReplyData(_testLinkShareDefinition);
        QCOMPARE(helper.shareCount(), 1);

        ShareModel model;
        QAbstractItemModelTester modelTester(&model);
        QCOMPARE(model.rowCount(), 0);

        QSignalSpy sharesChanged(&model, &ShareModel::sharesChanged);

        model.setAccountState(helper.accountState.data());
        model.setLocalPath(helper.fakeFolder.localPath() + helper.testFileName);

        QVERIFY(sharesChanged.wait(5000));
        QCOMPARE(helper.shareCount(), 1); // Check our test is working!
        QCOMPARE(model.rowCount(), helper.shareCount() + 1); // Internal link share!

        const auto shareIndex = model.index(model.rowCount() - 1, 0, {});
        QCOMPARE(shareIndex.data(ShareModel::LinkShareLabelRole).toBool(), true);

        const auto sharePtr = shareIndex.data(ShareModel::ShareRole).value<SharePtr>();
        const auto linkSharePtr = sharePtr.dynamicCast<LinkShare>(); // Need to connect to signal
        QSignalSpy labelSet(linkSharePtr.data(), &LinkShare::labelSet);
        const auto label = QStringLiteral("New link share label!");
        model.setLinkShareLabel(linkSharePtr, label);
        QVERIFY(labelSet.wait(3000));
        QCOMPARE(shareIndex.data(ShareModel::LinkShareLabelRole).toString(), label);
    }

    void testSharees()
    {
        helper.resetTestData();

        helper.appendShareReplyData(_testLinkShareDefinition);
        helper.appendShareReplyData(_testEmailShareDefinition);
        helper.appendShareReplyData(_testUserShareDefinition);
        QCOMPARE(helper.shareCount(), 3);

        ShareModel model;
        QAbstractItemModelTester modelTester(&model);
        QCOMPARE(model.rowCount(), 0);

        QSignalSpy sharesChanged(&model, &ShareModel::sharesChanged);

        model.setAccountState(helper.accountState.data());
        model.setLocalPath(helper.fakeFolder.localPath() + helper.testFileName);

        QVERIFY(sharesChanged.wait(5000));
        QCOMPARE(model.rowCount(), helper.shareCount() + 1); // Internal link share!

        QCOMPARE(model.sharees().count(), 2); // Link shares don't have sharees

        // Test adding a user group share -- we should still be showing a placeholder link share
        const ShareePtr sharee(new Sharee("testsharee@nextcloud.com", "Test sharee", Sharee::Type::Email));
        model.createNewUserGroupShare(sharee);
        QVERIFY(sharesChanged.wait(5000));
        QCOMPARE(helper.shareCount(), 4); // Check our test is working!
        QCOMPARE(model.rowCount(), helper.shareCount() + 1); // Internal link share!

        const auto sharees = model.sharees();
        QCOMPARE(sharees.count(), 3); // Link shares don't have sharees
        const auto lastSharee = sharees.last().value<ShareePtr>();
        QVERIFY(lastSharee);

        // Remove the user group share we just added
        const auto shareIndex = model.index(model.rowCount() - 1, 0, {});
        const auto sharePtr = shareIndex.data(ShareModel::ShareRole).value<SharePtr>();
        model.deleteShare(sharePtr);
        QVERIFY(sharesChanged.wait(5000));
        QCOMPARE(model.rowCount(), helper.shareCount() + 1); // Internal link share!

        // Now check the sharee is gone
        QCOMPARE(model.sharees().count(), 2);
    }

    void testSharePropertySetError()
    {
        helper.resetTestData();

        // Serve a broken share definition from the server to force an error
        auto brokenLinkShareDefinition = _testLinkShareDefinition;
        brokenLinkShareDefinition.shareId = QString();

        helper.appendShareReplyData(brokenLinkShareDefinition);
        QCOMPARE(helper.shareCount(), 1);

        ShareModel model;
        QAbstractItemModelTester modelTester(&model);
        QCOMPARE(model.rowCount(), 0);

        QSignalSpy sharesChanged(&model, &ShareModel::sharesChanged);

        model.setAccountState(helper.accountState.data());
        model.setLocalPath(helper.fakeFolder.localPath() + helper.testFileName);

        QVERIFY(sharesChanged.wait(5000));
        QCOMPARE(helper.shareCount(), 1); // Check our test is working!
        QCOMPARE(model.rowCount(), helper.shareCount() + 1); // Internal link share!

        // Reset the fake server to pretend like nothing is wrong there
        helper.resetTestShares();
        helper.appendShareReplyData(_testLinkShareDefinition);
        QCOMPARE(helper.shareCount(), 1);

        // Now try changing a property of the share
        const auto shareIndex = model.index(model.rowCount() - 1, 0, {});
        const auto share = shareIndex.data(ShareModel::ShareRole).value<SharePtr>();
        QSignalSpy serverError(&model, &ShareModel::serverError);

        model.toggleShareAllowEditing(share, false);
        QVERIFY(serverError.wait(3000));

        // Specific signal for password set error
        QSignalSpy passwordSetError(&model, &ShareModel::passwordSetError);
        const auto password = QStringLiteral("a pretty bad password but good thing it doesn't matter!");
        model.setSharePassword(share, password);
        QVERIFY(passwordSetError.wait(3000));
    }

};

QTEST_MAIN(TestShareModel)
#include "testsharemodel.moc"
