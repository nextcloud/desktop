/*
 * Copyright (C) 2022 by Claudio Cambra <claudio.cambra@nextcloud.com>
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

#include "gui/filedetails/shareemodel.h"

#include <QTest>
#include <QSignalSpy>

#include "accountmanager.h"
#include "syncenginetestutils.h"
#include "testhelper.h"

using namespace OCC;

static QByteArray fake400Response = R"(
{"ocs":{"meta":{"status":"failure","statuscode":400,"message":"Parameter is incorrect.\n"},"data":[]}}
)";

constexpr auto searchResultsReplyDelay = 100;

class TestShareeModel : public QObject
{
    Q_OBJECT

    int _numLookupSearchParamSet = 0;

public:
    ~TestShareeModel() override
    {
        AccountManager::instance()->deleteAccount(_accountState.data());
    };

    struct FakeShareeDefinition
    {
        QString label;

        QString shareWith;
        Sharee::Type type;
        QString shareWithAdditionalInfo;
    };

    void appendShareeToReply(const FakeShareeDefinition &definition)
    {
        QJsonObject newShareeJson;
        newShareeJson.insert("label", definition.label);

        QJsonObject newShareeValueJson;
        newShareeValueJson.insert("shareWith", definition.shareWith);
        newShareeValueJson.insert("shareType", definition.type);
        newShareeValueJson.insert("shareWithAdditionalInfo", definition.shareWithAdditionalInfo);

        newShareeJson.insert("value", newShareeValueJson);

        QString category;
        switch(definition.type) {
        case Sharee::Invalid:
            category = QStringLiteral("invalid");
            break;
        case Sharee::Circle:
            category = QStringLiteral("circles");
            break;
        case Sharee::Email:
            category = QStringLiteral("emails");
            break;
        case Sharee::Federated:
            category = QStringLiteral("remotes");
            break;
        case Sharee::Group:
            category = QStringLiteral("groups");
            break;
        case Sharee::Room:
            category = QStringLiteral("rooms");
            break;
        case Sharee::User:
            category = QStringLiteral("users");
            break;
        case Sharee::LookupServerSearch:
            category = QStringLiteral("placeholder_lookupserversearch");
            break;
        case Sharee::LookupServerSearchResults:
            category = QStringLiteral("placeholder_lookupserversearchresults");
            break;
        }

        auto shareesInCategory = _shareesMap.value(category).toJsonArray();
        shareesInCategory.append(newShareeJson);
        _shareesMap.insert(category, shareesInCategory);
    }

    void standardReplyPopulate()
    {
        appendShareeToReply(_michaelUserDefinition);
        appendShareeToReply(_liamUserDefinition);
        appendShareeToReply(_iqbalUserDefinition);
        appendShareeToReply(_universityGroupDefinition);
        appendShareeToReply(_testEmailDefinition);
    }

    QVariantMap filteredSharees(const QString &searchString)
    {
        if (searchString.isEmpty()) {
            return _shareesMap;
        }

        QVariantMap returnSharees;
        QJsonArray exactMatches;

        for (auto it = _shareesMap.constKeyValueBegin(); it != _shareesMap.constKeyValueEnd(); ++it) {
            const auto shareesCategory = it->first;
            const auto shareesArray = it->second.toJsonArray();
            QJsonArray filteredShareesArray;

            std::copy_if(shareesArray.cbegin(), shareesArray.cend(), std::back_inserter(filteredShareesArray), [&searchString](const QJsonValue &shareeValue) {
                const auto shareeObject = shareeValue.toObject().value("value").toObject();
                const auto shareeShareWith = shareeObject.value("shareWith").toString();
                return shareeShareWith.contains(searchString, Qt::CaseInsensitive);
            });

            std::copy_if(filteredShareesArray.cbegin(), filteredShareesArray.cend(), std::back_inserter(exactMatches), [&searchString](const QJsonValue &shareeValue) {
                const auto shareeObject = shareeValue.toObject().value("value").toObject();
                const auto shareeShareWith = shareeObject.value("shareWith").toString();
                return shareeShareWith == searchString;
            });

            returnSharees.insert(shareesCategory, filteredShareesArray);
        }

        returnSharees.insert(QStringLiteral("exact"), exactMatches);

        return returnSharees;
    }

    QByteArray testShareesReply(const QString &searchString)
    {
        QJsonObject root;
        QJsonObject ocs;
        QJsonObject meta;

        meta.insert("statuscode", 200);

        const auto resultSharees = filteredSharees(searchString);
        const auto shareesJsonObject = QJsonObject::fromVariantMap(resultSharees);

        ocs.insert(QStringLiteral("data"), shareesJsonObject);
        ocs.insert(QStringLiteral("meta"), meta);
        root.insert(QStringLiteral("ocs"), ocs);

        return QJsonDocument(root).toJson();
    }

    int shareesCount(const QString &searchString)
    {
        const auto sharees = filteredSharees(searchString);

        auto count = 0;
        const auto shareesCategories = sharees.values();
        for (const auto &shareesArrayValue : shareesCategories) {
            const auto shareesArray = shareesArrayValue.toJsonArray();
            count += shareesArray.count();
        }

        return count;
    }

    void resetTestData()
    {
        _alwaysReturnErrors = false;
        _shareesMap.clear();
    }


private:
    AccountPtr _account;
    AccountStatePtr _accountState;
    QScopedPointer<FakeQNAM> _fakeQnam;

    QVariantMap _shareesMap;

    // Some fake sharees of different categories
    // ALL OF THEM CONTAIN AN 'I' !! Important for testing
    FakeShareeDefinition _michaelUserDefinition {
        QStringLiteral("Michael"),
        QStringLiteral("michael"),
        Sharee::User,
        {},
    };
    FakeShareeDefinition _liamUserDefinition {
        QStringLiteral("Liam"),
        QStringLiteral("liam"),
        Sharee::User,
        {},
    };
    FakeShareeDefinition _iqbalUserDefinition {
        QStringLiteral("Iqbal"),
        QStringLiteral("iqbal"),
        Sharee::User,
        {},
    };

    FakeShareeDefinition _universityGroupDefinition {
        QStringLiteral("University"),
        QStringLiteral("university"),
        Sharee::Group,
        {},
    };

    FakeShareeDefinition _testEmailDefinition {
        QStringLiteral("test.email@nextcloud.com"),
        QStringLiteral("test.email@nextcloud.com"),
        Sharee::Email,
        {},
    };

    bool _alwaysReturnErrors = false;

private slots:
    void initTestCase()
    {
        _fakeQnam.reset(new FakeQNAM({}));
        _fakeQnam->setOverride([this](QNetworkAccessManager::Operation op, const QNetworkRequest &req, QIODevice *device) {
            Q_UNUSED(device);

            QNetworkReply *reply = nullptr;

            if (_alwaysReturnErrors) {
                reply = new FakeErrorReply(op, req, this, 400, fake400Response);
                return reply;
            }

            const auto reqUrl = req.url();
            const auto reqRawPath = reqUrl.path();
            const auto reqPath = reqRawPath.startsWith("/owncloud/") ? reqRawPath.mid(10) : reqRawPath;
            qDebug() << req.url() << reqPath << op;

            if(req.url().toString().startsWith(_accountState->account()->url().toString()) &&
                reqPath == QStringLiteral("ocs/v2.php/apps/files_sharing/api/v1/sharees") &&
                req.attribute(QNetworkRequest::CustomVerbAttribute) == "GET") {

                const auto urlQuery = QUrlQuery(req.url());
                const auto searchParam = urlQuery.queryItemValue(QStringLiteral("search"));
                const auto itemTypeParam = urlQuery.queryItemValue(QStringLiteral("itemType"));
                const auto pageParam = urlQuery.queryItemValue(QStringLiteral("page"));
                const auto perPageParam = urlQuery.queryItemValue(QStringLiteral("perPage"));
                const auto lookupParam = urlQuery.queryItemValue(QStringLiteral("lookup"));
                const auto formatParam = urlQuery.queryItemValue(QStringLiteral("format"));

                if (!lookupParam.isEmpty() && lookupParam == QStringLiteral("true")) {
                    ++_numLookupSearchParamSet;
                }

                if (formatParam != QStringLiteral("json")) {
                    reply = new FakeErrorReply(op, req, this, 400, fake400Response);
                } else {
                    reply = new FakePayloadReply(op, req, testShareesReply(searchParam), searchResultsReplyDelay, _fakeQnam.data());
                }
            }

            return reply;
        });

        _account = Account::create();
        _account->setCredentials(new FakeCredentials{_fakeQnam.data()});
        _account->setUrl(QUrl(("owncloud://somehost/owncloud")));
        _accountState = new AccountState(_account);
        AccountManager::instance()->addAccount(_account);

        // Let's verify our test is working -- all sharees have an I in their "shareWith"
        standardReplyPopulate();
        const auto searchString = QStringLiteral("i");
        QCOMPARE(shareesCount(searchString), 5);

        const auto emailSearchString = QStringLiteral("email");
        QCOMPARE(shareesCount(emailSearchString), 1);
    }

    void testSetAccountAndPath()
    {
        resetTestData();

        ShareeModel model;
        QAbstractItemModelTester modelTester(&model);
        QCOMPARE(model.rowCount(), 0);

        QSignalSpy accountStateChanged(&model, &ShareeModel::accountStateChanged);
        QSignalSpy shareItemIsFolderChanged(&model, &ShareeModel::shareItemIsFolderChanged);
        QSignalSpy searchStringChanged(&model, &ShareeModel::searchStringChanged);
        QSignalSpy lookupModeChanged(&model, &ShareeModel::lookupModeChanged);
        QSignalSpy shareeBlocklistChanged(&model, &ShareeModel::shareeBlocklistChanged);

        model.setAccountState(_accountState.data());
        QCOMPARE(accountStateChanged.count(), 1);
        QCOMPARE(model.accountState(), _accountState.data());

        const auto shareItemIsFolder = !model.shareItemIsFolder();
        model.setShareItemIsFolder(shareItemIsFolder);
        QCOMPARE(shareItemIsFolderChanged.count(), 1);
        QCOMPARE(model.shareItemIsFolder(), shareItemIsFolder);

        const auto searchString = QStringLiteral("search string");
        model.setSearchString(searchString);
        QCOMPARE(searchStringChanged.count(), 1);
        QCOMPARE(model.searchString(), searchString);

        const auto lookupMode = ShareeModel::LookupMode::GlobalSearch;
        model.setLookupMode(lookupMode);
        QCOMPARE(lookupModeChanged.count(), 1);
        QCOMPARE(model.lookupMode(), lookupMode);

        const ShareePtr sharee(new Sharee(_testEmailDefinition.shareWith, _testEmailDefinition.label, _testEmailDefinition.type));
        const QVariantList shareeBlocklist {QVariant::fromValue(sharee)};
        model.setShareeBlocklist(shareeBlocklist);
        QCOMPARE(shareeBlocklistChanged.count(), 1);
        QCOMPARE(model.shareeBlocklist(), shareeBlocklist);
    }

    void testShareesFetch()
    {
        resetTestData();
        standardReplyPopulate();

        ShareeModel model;
        QAbstractItemModelTester modelTester(&model);
        QCOMPARE(model.rowCount(), 0);

        model.setAccountState(_accountState.data());

        QSignalSpy shareesReady(&model, &ShareeModel::shareesReady);
        const auto searchString = QStringLiteral("i");
        model.setSearchString(searchString);
        QVERIFY(shareesReady.wait(3000));
        QCOMPARE(model.rowCount(), shareesCount(searchString) + 1);
        QVERIFY(model.rowCount() > 0);
        auto lastElementType = model.data(model.index(model.rowCount() - 1), ShareeModel::Roles::TypeRole).toInt();
        QVERIFY(lastElementType == Sharee::Type::LookupServerSearch);

        const auto emailSearchString = QStringLiteral("email");
        model.setSearchString(emailSearchString);
        QVERIFY(shareesReady.wait(3000));
        QCOMPARE(model.rowCount(), shareesCount(emailSearchString) + 1);
        QVERIFY(model.rowCount() > 0);
        lastElementType = model.data(model.index(model.rowCount() - 1), ShareeModel::Roles::TypeRole).toInt();
        QVERIFY(lastElementType == Sharee::Type::LookupServerSearch);
    }

    void testShareesFetchGlobally()
    {
        resetTestData();
        standardReplyPopulate();

        ShareeModel model;
        QAbstractItemModelTester modelTester(&model);
        QCOMPARE(model.rowCount(), 0);

        model.setAccountState(_accountState.data());

        QSignalSpy shareesReady(&model, &ShareeModel::shareesReady);
        const auto emailSearchString = QStringLiteral("email");
        model.setSearchString(emailSearchString);
        QVERIFY(shareesReady.wait(3000));
        QCOMPARE(model.rowCount(), shareesCount(emailSearchString) + 1);
        QVERIFY(model.rowCount() > 0);
        auto lastElementType = model.data(model.index(model.rowCount() - 1), ShareeModel::Roles::TypeRole).toInt();
        QVERIFY(lastElementType == Sharee::Type::LookupServerSearch);
        QCOMPARE(_numLookupSearchParamSet, 0);

        QSignalSpy lookupModeChanged(&model, &ShareeModel::lookupModeChanged);
        model.searchGlobally();
        QVERIFY(shareesReady.wait(3000));
        QCOMPARE(lookupModeChanged.count(), 2);
        QVERIFY(model.lookupMode() == ShareeModel::LookupMode::LocalSearch);
        QCOMPARE(model.rowCount(), shareesCount(emailSearchString) + 1);
        QVERIFY(model.rowCount() > 0);
        lastElementType = model.data(model.index(model.rowCount() - 1), ShareeModel::Roles::TypeRole).toInt();
        QVERIFY(lastElementType == Sharee::Type::LookupServerSearchResults);
        QCOMPARE(_numLookupSearchParamSet, 1);
    }

    void testFetchSignalling()
    {
        resetTestData();
        standardReplyPopulate();

        ShareeModel model;
        QAbstractItemModelTester modelTester(&model);
        QCOMPARE(model.rowCount(), 0);

        model.setAccountState(_accountState.data());
        QSignalSpy fetchOngoingChanged(&model, &ShareeModel::fetchOngoingChanged);
        const auto searchString = QStringLiteral("i");
        model.setSearchString(searchString);

        QVERIFY(fetchOngoingChanged.wait(1000));
        QCOMPARE(model.fetchOngoing(), true);
        QVERIFY(fetchOngoingChanged.wait(3000));
        QCOMPARE(model.fetchOngoing(), false);
    }

    void testData()
    {
        resetTestData();
        appendShareeToReply(_testEmailDefinition);

        ShareeModel model;
        QAbstractItemModelTester modelTester(&model);
        QCOMPARE(model.rowCount(), 0);

        model.setAccountState(_accountState.data());
        const auto searchString = QStringLiteral("i");
        model.setSearchString(searchString);

        QSignalSpy shareesReady(&model, &ShareeModel::shareesReady);
        QVERIFY(shareesReady.wait(3000));
        QCOMPARE(model.rowCount(), shareesCount(searchString) + 1);
        auto lastElementType = model.data(model.index(model.rowCount() - 1), ShareeModel::Roles::TypeRole).toInt();
        QVERIFY(lastElementType == Sharee::Type::LookupServerSearch);

        const auto shareeIndex = model.index(0, 0, {});

        const ShareePtr expectedSharee(new Sharee(_testEmailDefinition.shareWith, _testEmailDefinition.label, _testEmailDefinition.type));
        const auto sharee = shareeIndex.data(ShareeModel::ShareeRole).value<ShareePtr>();
        QCOMPARE(sharee->format(), expectedSharee->format());
        QCOMPARE(sharee->shareWith(), expectedSharee->shareWith());
        QCOMPARE(sharee->displayName(), expectedSharee->displayName());
        QCOMPARE(sharee->type(), expectedSharee->type());

        const auto expectedShareeDisplay = QString(_testEmailDefinition.label + QStringLiteral(" (email)"));
        const auto shareeDisplay = shareeIndex.data(Qt::DisplayRole).toString();
        QCOMPARE(shareeDisplay, expectedShareeDisplay);

        const auto expectedAutoCompleterStringMatch = QString(_testEmailDefinition.label +
                                                              QStringLiteral(" (") +
                                                              _testEmailDefinition.shareWith +
                                                              QStringLiteral(")"));
        const auto autoCompleterStringMatch = shareeIndex.data(ShareeModel::AutoCompleterStringMatchRole).toString();
        QCOMPARE(autoCompleterStringMatch, expectedAutoCompleterStringMatch);
    }

    void testBlocklist()
    {
        resetTestData();
        standardReplyPopulate();

        ShareeModel model;
        QAbstractItemModelTester modelTester(&model);
        QCOMPARE(model.rowCount(), 0);

        model.setAccountState(_accountState.data());

        const ShareePtr sharee(new Sharee(_testEmailDefinition.shareWith, _testEmailDefinition.label, _testEmailDefinition.type));
        const QVariantList shareeBlocklist {QVariant::fromValue(sharee)};
        model.setShareeBlocklist(shareeBlocklist);

        QSignalSpy shareesReady(&model, &ShareeModel::shareesReady);
        const auto searchString = QStringLiteral("i");
        model.setSearchString(searchString);
        QVERIFY(shareesReady.wait(3000));
        QCOMPARE(model.rowCount(), shareesCount(searchString) - 1 + 1);
        auto lastElementType = model.data(model.index(model.rowCount() - 1), ShareeModel::Roles::TypeRole).toInt();
        QVERIFY(lastElementType == Sharee::Type::LookupServerSearch);

        const ShareePtr shareeTwo(new Sharee(_michaelUserDefinition.shareWith, _michaelUserDefinition.label, _michaelUserDefinition.type));
        const QVariantList largerShareeBlocklist {QVariant::fromValue(sharee), QVariant::fromValue(shareeTwo)};
        model.setShareeBlocklist(largerShareeBlocklist);
        QCOMPARE(model.rowCount(), shareesCount(searchString) - 2 + 1);
        lastElementType = model.data(model.index(model.rowCount() - 1), ShareeModel::Roles::TypeRole).toInt();
        QVERIFY(lastElementType == Sharee::Type::LookupServerSearch);
    }

    void testServerError()
    {
        resetTestData();
        _alwaysReturnErrors = true;

        ShareeModel model;
        QAbstractItemModelTester modelTester(&model);
        QCOMPARE(model.rowCount(), 0);

        model.setAccountState(_accountState.data());

        QSignalSpy displayErrorMessage(&model, &ShareeModel::displayErrorMessage);
        QSignalSpy fetchOngoingChanged(&model, &ShareeModel::fetchOngoingChanged);
        model.setSearchString(QStringLiteral("i"));
        QVERIFY(displayErrorMessage.wait(3000));

        QCOMPARE(fetchOngoingChanged.count(), 2);
        QCOMPARE(model.fetchOngoing(), false);
    }
};

QTEST_MAIN(TestShareeModel)
#include "testshareemodel.moc"
