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

#include "gui/filetagmodel.h"

#include <QTest>
#include <QSignalSpy>
#include <QDomDocument>
#include <QAbstractItemModelTester>

#include "accountmanager.h"
#include "syncenginetestutils.h"

using namespace OCC;

namespace {
const auto testTaglessXmlResponse = QByteArray("<?xml version='1.0'?>\n<d:multistatus xmlns:d=\"DAV:\">\n  <d:response xmlns:d=\"DAV:\">\n    <d:href xmlns:d=\"DAV:\">/remote.php/dav/files/tag/Documents/</d:href>\n    <d:propstat xmlns:d=\"DAV:\">\n      <d:prop xmlns:d=\"DAV:\">\n        <nc:tags xmlns:nc=\"http://nextcloud.org/ns\"/>\n        <nc:system-tags xmlns:nc=\"http://nextcloud.org/ns\"/>\n      </d:prop>\n      <d:status xmlns:d=\"DAV:\">HTTP/1.1 404 Not Found</d:status>\n    </d:propstat>\n  </d:response>\n</d:multistatus>\n");
const auto testSystemAndNormalTagsOnlyXmlResponse = QByteArray("<?xml version='1.0'?>\n<d:multistatus xmlns:d=\"DAV:\">\n  <d:response xmlns:d=\"DAV:\">\n    <d:href xmlns:d=\"DAV:\">/remote.php/dav/files/tag/Documents/</d:href>\n    <d:propstat xmlns:d=\"DAV:\">\n      <d:prop xmlns:d=\"DAV:\">\n        <nc:tags xmlns:nc=\"http://nextcloud.org/ns\">\n          <oc:tag xmlns:oc=\"http://owncloud.org/ns\">test 0</oc:tag>\n          <oc:tag xmlns:oc=\"http://owncloud.org/ns\">test 1</oc:tag>\n          <oc:tag xmlns:oc=\"http://owncloud.org/ns\">test 2</oc:tag>\n          <oc:tag xmlns:oc=\"http://owncloud.org/ns\">test 3</oc:tag>\n        </nc:tags>\n        <nc:system-tags xmlns:nc=\"http://nextcloud.org/ns\">\n          <nc:system-tag xmlns:nc=\"http://nextcloud.org/ns\" oc:can-assign=\"true\" xmlns:oc=\"http://owncloud.org/ns\" oc:user-assignable=\"true\" oc:id=\"3\" oc:user-visible=\"true\">important</nc:system-tag>\n          <nc:system-tag xmlns:nc=\"http://nextcloud.org/ns\" oc:can-assign=\"true\" xmlns:oc=\"http://owncloud.org/ns\" oc:user-assignable=\"true\" oc:id=\"4\" oc:user-visible=\"true\">marino</nc:system-tag>\n          <nc:system-tag xmlns:nc=\"http://nextcloud.org/ns\" oc:can-assign=\"true\" xmlns:oc=\"http://owncloud.org/ns\" oc:user-assignable=\"true\" oc:id=\"5\" oc:user-visible=\"true\">marino2</nc:system-tag>\n          <nc:system-tag xmlns:nc=\"http://nextcloud.org/ns\" oc:can-assign=\"true\" xmlns:oc=\"http://owncloud.org/ns\" oc:user-assignable=\"true\" oc:id=\"1\" oc:user-visible=\"true\">one</nc:system-tag>\n          <nc:system-tag xmlns:nc=\"http://nextcloud.org/ns\" oc:can-assign=\"true\" xmlns:oc=\"http://owncloud.org/ns\" oc:user-assignable=\"true\" oc:id=\"2\" oc:user-visible=\"true\">two</nc:system-tag>\n        </nc:system-tags>\n      </d:prop>\n      <d:status xmlns:d=\"DAV:\">HTTP/1.1 200 OK</d:status>\n    </d:propstat>\n  </d:response>\n</d:multistatus>\n");
const auto testTagsOnlyXmlResponse = QByteArray("<?xml version='1.0'?>\n<d:multistatus xmlns:d=\"DAV:\">\n  <d:response xmlns:d=\"DAV:\">\n    <d:href xmlns:d=\"DAV:\">/remote.php/dav/files/tag/Documents/</d:href>\n    <d:propstat xmlns:d=\"DAV:\">\n      <d:prop xmlns:d=\"DAV:\">\n        <nc:tags xmlns:nc=\"http://nextcloud.org/ns\">\n          <oc:tag xmlns:oc=\"http://owncloud.org/ns\">test 0</oc:tag>\n          <oc:tag xmlns:oc=\"http://owncloud.org/ns\">test 1</oc:tag>\n          <oc:tag xmlns:oc=\"http://owncloud.org/ns\">test 2</oc:tag>\n          <oc:tag xmlns:oc=\"http://owncloud.org/ns\">test 3</oc:tag>\n        </nc:tags>\n      </d:prop>\n      <d:status xmlns:d=\"DAV:\">HTTP/1.1 200 OK</d:status>\n    </d:propstat>\n  </d:response>\n</d:multistatus>\n");
const auto testSystemTagsOnlyXmlResponse = QByteArray("<?xml version='1.0'?>\n<d:multistatus xmlns:d=\"DAV:\">\n  <d:response xmlns:d=\"DAV:\">\n    <d:href xmlns:d=\"DAV:\">/remote.php/dav/files/tag/Documents/</d:href>\n    <d:propstat xmlns:d=\"DAV:\">\n      <d:prop xmlns:d=\"DAV:\">\n        <nc:system-tags xmlns:nc=\"http://nextcloud.org/ns\">\n          <nc:system-tag xmlns:nc=\"http://nextcloud.org/ns\" oc:can-assign=\"true\" xmlns:oc=\"http://owncloud.org/ns\" oc:user-assignable=\"true\" oc:id=\"3\" oc:user-visible=\"true\">important</nc:system-tag>\n          <nc:system-tag xmlns:nc=\"http://nextcloud.org/ns\" oc:can-assign=\"true\" xmlns:oc=\"http://owncloud.org/ns\" oc:user-assignable=\"true\" oc:id=\"4\" oc:user-visible=\"true\">marino</nc:system-tag>\n          <nc:system-tag xmlns:nc=\"http://nextcloud.org/ns\" oc:can-assign=\"true\" xmlns:oc=\"http://owncloud.org/ns\" oc:user-assignable=\"true\" oc:id=\"5\" oc:user-visible=\"true\">marino2</nc:system-tag>\n          <nc:system-tag xmlns:nc=\"http://nextcloud.org/ns\" oc:can-assign=\"true\" xmlns:oc=\"http://owncloud.org/ns\" oc:user-assignable=\"true\" oc:id=\"1\" oc:user-visible=\"true\">one</nc:system-tag>\n          <nc:system-tag xmlns:nc=\"http://nextcloud.org/ns\" oc:can-assign=\"true\" xmlns:oc=\"http://owncloud.org/ns\" oc:user-assignable=\"true\" oc:id=\"2\" oc:user-visible=\"true\">two</nc:system-tag>\n        </nc:system-tags>\n      </d:prop>\n      <d:status xmlns:d=\"DAV:\">HTTP/1.1 200 OK</d:status>\n    </d:propstat>\n  </d:response>\n</d:multistatus>\n");
const auto testErrorXmlResponse = QByteArray("<?xml version='1.0'?>\n<d:multistatus xmlns:d=\"DAV:\">\n  <d:response xmlns:d=\"DAV:\">\n    <d:href xmlns:d=\"DAV:\">/remote.php/dav/files/tag/</d:href>\n    <d:propstat xmlns:d=\"DAV:\">\n      <d:prop xmlns:d=\"DAV:\">\n        <d:getlastmodified xmlns:d=\"DAV:\">Wed, 19 Apr 2023 15:09:16 GMT</d:getlastmodified>\n        <d:resourcetype xmlns:d=\"DAV:\">\n          <d:collection xmlns:d=\"DAV:\"/>\n        </d:resourcetype>\n        <d:quota-used-bytes xmlns:d=\"DAV:\">25789373</d:quota-used-bytes>\n        <d:quota-available-bytes xmlns:d=\"DAV:\">-3</d:quota-available-bytes>\n        <d:getetag xmlns:d=\"DAV:\">\"083237f7b434afbedeace283d655cc41\"</d:getetag>\n      </d:prop>\n      <d:status xmlns:d=\"DAV:\">HTTP/1.1 200 OK</d:status>\n    </d:propstat>\n  </d:response>\n</d:multistatus>\n");
const QString testUser = "admin";
const QString testFilePath = "Documents";
const QString testUrlPath = "/remote.php/dav/files/" + testUser + '/' + testFilePath;
constexpr auto testNumTags = 9;

}

class TestFileTagModel : public QObject
{
    Q_OBJECT

private:
    AccountPtr _account;
    AccountStatePtr _accountState;
    QScopedPointer<FakeQNAM> _fakeQnam;
    QStringList _expectedTags;

private slots:
    void initTestCase()
    {
        _fakeQnam.reset(new FakeQNAM({}));
        _fakeQnam->setOverride([this](QNetworkAccessManager::Operation op, const QNetworkRequest &req, QIODevice * const device) {
            Q_UNUSED(device);
            QNetworkReply *reply = nullptr;

            const auto path = req.url().path();

            auto requestDom = QDomDocument();
            const auto parsedCorrectly = requestDom.setContent(device);
            const auto tagElems = requestDom.elementsByTagName("tags");
            const auto systemTagElems = requestDom.elementsByTagName("system-tags");

            if (!parsedCorrectly || !req.url().toString().startsWith(_accountState->account()->url().toString())) {
                reply = new FakePropfindReply(testErrorXmlResponse, op, req, this);
            }

            if (path.contains(testUrlPath)) {
                if (tagElems.count() > 0 && systemTagElems.count() > 0) {
                    reply = new FakePropfindReply(testSystemAndNormalTagsOnlyXmlResponse, op, req, this);
                } else if (tagElems.count() > 0) {
                    reply = new FakePropfindReply(testTagsOnlyXmlResponse, op, req, this);
                } else if (systemTagElems.count() > 0) {
                    reply = new FakePropfindReply(testSystemTagsOnlyXmlResponse, op, req, this);
                } else {
                    reply = new FakePropfindReply(testTaglessXmlResponse, op, req, this);
                }
            }

            if (!reply) {
                reply = new FakePropfindReply(testErrorXmlResponse, op, req, this);
            }

            return reply;
        });

        _account = Account::create();
        _account->setCredentials(new FakeCredentials{_fakeQnam.data()});
        _account->setUrl(QUrl(("owncloud://somehost/owncloud")));
        _accountState = new AccountState(_account);
        AccountManager::instance()->addAccount(_account);

        _expectedTags = QStringList{
            "test 0",
            "test 1",
            "test 2",
            "test 3",
            "important",
            "marino",
            "marino2",
            "one",
            "two"
        };
    }

    void testModelMainProps()
    {
        auto fileTagModel = FileTagModel(testFilePath, _account);
        const auto fileTagModelTester = QAbstractItemModelTester(&fileTagModel);
        QSignalSpy fileTagsChanged(&fileTagModel, &FileTagModel::totalTagsChanged);
        fileTagsChanged.wait(1000);

        QCOMPARE(fileTagModel.serverRelativePath(), testFilePath);
        QCOMPARE(fileTagModel.account(), _account);

        QSignalSpy serverRelativePathChangedSpy(&fileTagModel, &FileTagModel::serverRelativePathChanged);
        fileTagModel.setServerRelativePath("");
        QCOMPARE(serverRelativePathChangedSpy.count(), 1);
        QCOMPARE(fileTagModel.serverRelativePath(), "");

        QSignalSpy accountChangedSpy(&fileTagModel, &FileTagModel::accountChanged);
        const AccountPtr testAccount;
        fileTagModel.setAccount(testAccount);
        QCOMPARE(accountChangedSpy.count(), 1);
        QCOMPARE(fileTagModel.account(), testAccount);
    }

    void testModelTagFetch()
    {
        auto fileTagModel = FileTagModel(testFilePath, _account);
        const auto fileTagModelTester = QAbstractItemModelTester(&fileTagModel);
        QSignalSpy fileTagsChanged(&fileTagModel, &FileTagModel::totalTagsChanged);
        fileTagsChanged.wait(1000);

        const auto modelTotalTags = fileTagModel.totalTags();
        QCOMPARE(modelTotalTags, testNumTags);

        for (auto i = 0; i < modelTotalTags; ++i) {
            const auto index = fileTagModel.index(i);
            const auto tag = index.data();

            QCOMPARE(tag.toString(), _expectedTags[i]);
        }
    }

    void testModelMaxTags()
    {
        auto fileTagModel = FileTagModel(testFilePath, _account);
        const auto fileTagModelTester = QAbstractItemModelTester(&fileTagModel);
        QSignalSpy fileTagsChanged(&fileTagModel, &FileTagModel::totalTagsChanged);
        fileTagsChanged.wait(1000);

        constexpr auto testMaxTags = 3;
        Q_ASSERT(testMaxTags < testNumTags);

        QSignalSpy maxTagsChangedSpy(&fileTagModel, &FileTagModel::maxTagsChanged);
        fileTagModel.setMaxTags(testMaxTags);
        QCOMPARE(maxTagsChangedSpy.count(), 1);
        QCOMPARE(fileTagModel.maxTags(), testMaxTags);
        QCOMPARE(fileTagModel.rowCount(), testMaxTags);
    }
};

QTEST_MAIN(TestFileTagModel)
#include "testfiletagmodel.moc"
