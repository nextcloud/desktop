/*
 * Copyright (C) by Oleksandr Zolotov <alex@nextcloud.com>
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

#include "gui/tray/unifiedsearchresultslistmodel.h"

#include "account.h"
#include "accountstate.h"
#include "syncenginetestutils.h"

#include <QAbstractItemModelTester>
#include <QDesktopServices>
#include <QSignalSpy>
#include <QTest>

namespace {
/**
 * @brief The FakeDesktopServicesUrlHandler
 * overrides QDesktopServices::openUrl
 **/
class FakeDesktopServicesUrlHandler : public QObject
{
    Q_OBJECT

public:
    FakeDesktopServicesUrlHandler(QObject *parent = nullptr)
        : QObject(parent)
    {}

public:
signals:
    void resultClicked(const QUrl &url);
};

/**
 * @brief The FakeProvider
 * is a simple structure that represents initial list of providers and their properties
 **/
class FakeProvider
{
public:
    QString _id;
    QString _name;
    qint32 _order = std::numeric_limits<qint32>::max();
    quint32 _numItemsToInsert = 5; // how many fake results to insert
};

// this will be used when initializing fake search results data for each provider
static const QVector<FakeProvider> fakeProvidersInitInfo = {
    {QStringLiteral("settings_apps"), QStringLiteral("Apps"), -50, 10},
    {QStringLiteral("talk-message"), QStringLiteral("Messages"), -2, 17},
    {QStringLiteral("files"), QStringLiteral("Files"), 5, 3},
    {QStringLiteral("deck"), QStringLiteral("Deck"), 10, 5},
    {QStringLiteral("comments"), QStringLiteral("Comments"), 10, 2},
    {QStringLiteral("mail"), QStringLiteral("Mails"), 10, 15},
    {QStringLiteral("calendar"), QStringLiteral("Events"), 30, 11}
};

static QByteArray fake404Response = R"(
{"ocs":{"meta":{"status":"failure","statuscode":404,"message":"Invalid query, please check the syntax. API specifications are here: http:\/\/www.freedesktop.org\/wiki\/Specifications\/open-collaboration-services.\n"},"data":[]}}
)";

static QByteArray fake400Response = R"(
{"ocs":{"meta":{"status":"failure","statuscode":400,"message":"Parameter is incorrect.\n"},"data":[]}}
)";

static QByteArray fake500Response = R"(
{"ocs":{"meta":{"status":"failure","statuscode":500,"message":"Internal Server Error.\n"},"data":[]}}
)";

/**
 * @brief The FakeSearchResultsStorage
 * emulates the real server storage that contains all the results that UnifiedSearchListmodel will search for
 **/
class FakeSearchResultsStorage
{
    class Provider
    {
    public:
        class SearchResult
        {
        public:
            QString _thumbnailUrl;
            QString _title;
            QString _subline;
            QString _resourceUrl;
            QString _icon;
            bool _rounded;
        };

        QString _id;
        QString _name;
        qint32 _order = std::numeric_limits<qint32>::max();
        qint32 _cursor = 0;
        bool _isPaginated = false;
        QVector<SearchResult> _results;
    };

    FakeSearchResultsStorage() = default;

public:
    static FakeSearchResultsStorage *instance()
    {
        if (!_instance) {
            _instance = new FakeSearchResultsStorage();
            _instance->init();
        }

        return _instance;
    };

    static void destroy()
    {
        if (_instance) {
            delete _instance;
        }

        _instance = nullptr;
    }

    void init()
    {
        if (!_searchResultsData.isEmpty()) {
            return;
        }

        _metaSuccess = {{QStringLiteral("status"), QStringLiteral("ok")}, {QStringLiteral("statuscode"), 200},
            {QStringLiteral("message"), QStringLiteral("OK")}};

        initProvidersResponse();

        initSearchResultsData();
    }

    // initialize the JSON response containing the fake list of providers and their properties
    void initProvidersResponse()
    {
        QList<QVariant> providersList;

        for (const auto &fakeProviderInitInfo : fakeProvidersInitInfo) {
            providersList.push_back(QVariantMap{
                {QStringLiteral("id"), fakeProviderInitInfo._id},
                {QStringLiteral("name"), fakeProviderInitInfo._name},
                {QStringLiteral("order"), fakeProviderInitInfo._order},
            });
        }

        const QVariantMap ocsMap = {
            {QStringLiteral("meta"), _metaSuccess},
            {QStringLiteral("data"), providersList}
        };

        _providersResponse =
            QJsonDocument::fromVariant(QVariantMap{{QStringLiteral("ocs"), ocsMap}}).toJson(QJsonDocument::Compact);
    }

    // init the map of fake search results for each provider
    void initSearchResultsData()
    {
        for (const auto &fakeProvider : fakeProvidersInitInfo) {
            auto &providerData = _searchResultsData[fakeProvider._id];
            providerData._id = fakeProvider._id;
            providerData._name = fakeProvider._name;
            providerData._order = fakeProvider._order;
            if (fakeProvider._numItemsToInsert > pageSize) {
                providerData._isPaginated = true;
            }
            for (quint32 i = 0; i < fakeProvider._numItemsToInsert; ++i) {
                providerData._results.push_back(
                    {"http://example.de/avatar/john/64", QString(QStringLiteral("John Doe in ") + fakeProvider._name),
                        QString(QStringLiteral("We a discussion about ") + fakeProvider._name
                            + QStringLiteral(" already. But, let's have a follow up tomorrow afternoon.")),
                        "http://example.de/call/abcde12345#message_12345", QStringLiteral("icon-talk"), true});
            }
        }
    }

    const QList<QVariant> resultsForProvider(const QString &providerId, int cursor)
    {
        QList<QVariant> list;

        const auto results = resultsForProviderAsVector(providerId, cursor);

        if (results.isEmpty()) {
            return list;
        }

        for (const auto &result : results) {
            list.push_back(QVariantMap{
                {"thumbnailUrl", result._thumbnailUrl},
                {"title", result._title},
                {"subline", result._subline},
                {"resourceUrl", result._resourceUrl},
                {"icon", result._icon},
                {"rounded", result._rounded}
            });
        }

        return list;
    }

    const QVector<Provider::SearchResult> resultsForProviderAsVector(const QString &providerId, int cursor)
    {
        QVector<Provider::SearchResult> results;

        const auto provider = _searchResultsData.value(providerId, Provider());

        if (provider._id.isEmpty() || cursor > provider._results.size()) {
            return results;
        }

        const int n = cursor + pageSize > provider._results.size()
            ? 0
            : cursor + pageSize;

        for (int i = cursor; i < n; ++i) {
            results.push_back(provider._results[i]);
        }

        return results;
    }

    const QByteArray queryProvider(const QString &providerId, const QString &searchTerm, int cursor)
    {
        if (!_searchResultsData.contains(providerId)) {
            return fake404Response;
        }

        if (searchTerm == QStringLiteral("[HTTP500]")) {
            return fake500Response;
        }

        if (searchTerm == QStringLiteral("[empty]")) {
            const QVariantMap dataMap = {{QStringLiteral("name"), _searchResultsData[providerId]._name},
                {QStringLiteral("isPaginated"), false}, {QStringLiteral("cursor"), 0},
                {QStringLiteral("entries"), QVariantList{}}};

            const QVariantMap ocsMap = {{QStringLiteral("meta"), _metaSuccess}, {QStringLiteral("data"), dataMap}};

            return QJsonDocument::fromVariant(QVariantMap{{QStringLiteral("ocs"), ocsMap}})
                .toJson(QJsonDocument::Compact);
        }

        const auto provider = _searchResultsData.value(providerId, Provider());

        const auto nextCursor = cursor + pageSize;

        const QVariantMap dataMap = {{QStringLiteral("name"), _searchResultsData[providerId]._name},
            {QStringLiteral("isPaginated"), _searchResultsData[providerId]._isPaginated},
            {QStringLiteral("cursor"), nextCursor},
            {QStringLiteral("entries"), resultsForProvider(providerId, cursor)}};

        const QVariantMap ocsMap = {{QStringLiteral("meta"), _metaSuccess}, {QStringLiteral("data"), dataMap}};

        return QJsonDocument::fromVariant(QVariantMap{{QStringLiteral("ocs"), ocsMap}}).toJson(QJsonDocument::Compact);
    }

    [[nodiscard]] const QByteArray &fakeProvidersResponseJson() const { return _providersResponse; }

private:
    static FakeSearchResultsStorage *_instance;

    static const int pageSize = 5;

    QMap<QString, Provider> _searchResultsData;

    QByteArray _providersResponse = fake404Response;

    QVariantMap _metaSuccess;
};

FakeSearchResultsStorage *FakeSearchResultsStorage::_instance = nullptr;

}

class TestUnifiedSearchListmodel : public QObject
{
    Q_OBJECT

public:
    TestUnifiedSearchListmodel() = default;

    QScopedPointer<FakeQNAM> fakeQnam;
    OCC::AccountPtr account;
    QScopedPointer<OCC::AccountState> accountState;
    QScopedPointer<OCC::UnifiedSearchResultsListModel> model;
    QScopedPointer<QAbstractItemModelTester> modelTester;

    QScopedPointer<FakeDesktopServicesUrlHandler> fakeDesktopServicesUrlHandler;

    static const int searchResultsReplyDelay = 100;

private slots:
    void initTestCase()
    {
        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);

        QStandardPaths::setTestModeEnabled(true);

        fakeQnam.reset(new FakeQNAM({}));
        account = OCC::Account::create();
        account->setCredentials(new FakeCredentials{fakeQnam.data()});
        account->setUrl(QUrl(("http://example.de")));

        accountState.reset(new OCC::AccountState(account));

        fakeQnam->setOverride([this](QNetworkAccessManager::Operation op, const QNetworkRequest &req, QIODevice *device) {
            Q_UNUSED(device);
            QNetworkReply *reply = nullptr;

            const auto urlQuery = QUrlQuery(req.url());
            const auto format = urlQuery.queryItemValue(QStringLiteral("format"));
            const auto cursor = urlQuery.queryItemValue(QStringLiteral("cursor")).toInt();
            const auto searchTerm = urlQuery.queryItemValue(QStringLiteral("term"));
            const auto path = req.url().path();

            if (!req.url().toString().startsWith(accountState->account()->url().toString())) {
                reply = new FakeErrorReply(op, req, this, 404, fake404Response);
            }
            if (format != QStringLiteral("json")) {
                reply = new FakeErrorReply(op, req, this, 400, fake400Response);
            }

            // handle fetch of providers list
            if (path.startsWith(QStringLiteral("/ocs/v2.php/search/providers")) && searchTerm.isEmpty()) {
                reply = new FakePayloadReply(op, req,
                    FakeSearchResultsStorage::instance()->fakeProvidersResponseJson(), fakeQnam.data());
            // handle search for provider
            } else if (path.startsWith(QStringLiteral("/ocs/v2.php/search/providers")) && !searchTerm.isEmpty()) {
                const auto pathSplit = path.mid(QString(QStringLiteral("/ocs/v2.php/search/providers")).size())
                                           .split(QLatin1Char('/'), Qt::SkipEmptyParts);

                if (!pathSplit.isEmpty() && path.contains(pathSplit.first())) {
                    reply = new FakePayloadReply(op, req,
                        FakeSearchResultsStorage::instance()->queryProvider(pathSplit.first(), searchTerm, cursor),
                        searchResultsReplyDelay, fakeQnam.data());
                }
            }

            if (!reply) {
                return qobject_cast<QNetworkReply*>(new FakeErrorReply(op, req, this, 404, QByteArrayLiteral("{error: \"Not found!\"}")));
            }

            return reply;
        });

        model.reset(new OCC::UnifiedSearchResultsListModel(accountState.data()));

        modelTester.reset(new QAbstractItemModelTester(model.data()));

        fakeDesktopServicesUrlHandler.reset(new FakeDesktopServicesUrlHandler);
    }
    void testSetSearchTermStartStopSearch()
    {
        // make sure the model is empty
        model->setSearchTerm(QStringLiteral(""));
        QVERIFY(model->rowCount() == 0);

        // #1 test setSearchTerm actually sets the search term and the signal is emitted
        QSignalSpy searhTermChanged(model.data(), &OCC::UnifiedSearchResultsListModel::searchTermChanged);
        model->setSearchTerm(QStringLiteral("dis"));
        QCOMPARE(searhTermChanged.count(), 1);
        QCOMPARE(model->searchTerm(), QStringLiteral("dis"));

        // #2 test setSearchTerm actually sets the search term and the signal is emitted
        searhTermChanged.clear();
        model->setSearchTerm(model->searchTerm() + QStringLiteral("cuss"));
        QCOMPARE(model->searchTerm(), QStringLiteral("discuss"));
        QCOMPARE(searhTermChanged.count(), 1);

        // #3 test that model has not started search yet
        QVERIFY(!model->isSearchInProgress());

        
        // #4 test that model has started the search after specific delay
        QSignalSpy searchInProgressChanged(model.data(), &OCC::UnifiedSearchResultsListModel::isSearchInProgressChanged);
        // allow search jobs to get created within the model
        QVERIFY(searchInProgressChanged.wait());
        QCOMPARE(searchInProgressChanged.count(), 1);
        QVERIFY(model->isSearchInProgress());

        // #5 test that model has stopped the search after setting empty search term
        model->setSearchTerm(QStringLiteral(""));
        QVERIFY(!model->isSearchInProgress());
    }

    void testSetSearchTermResultsFound()
    {
        // make sure the model is empty
        model->setSearchTerm(QStringLiteral(""));
        QVERIFY(model->rowCount() == 0);

        // test that search term gets set, search gets started and enough results get returned
        model->setSearchTerm(model->searchTerm() + QStringLiteral("discuss"));

        QSignalSpy searchInProgressChanged(
            model.data(), &OCC::UnifiedSearchResultsListModel::isSearchInProgressChanged);

        QVERIFY(searchInProgressChanged.wait());

        // make sure search has started
        QCOMPARE(searchInProgressChanged.count(), 1);
        QVERIFY(model->isSearchInProgress());

        QVERIFY(searchInProgressChanged.wait());

        // make sure search has finished
        QVERIFY(!model->isSearchInProgress());

        QVERIFY(model->rowCount() > 0);
    }

    void testSetSearchTermResultsNotFound()
    {
        // make sure the model is empty
        model->setSearchTerm(QStringLiteral(""));
        QVERIFY(model->rowCount() == 0);

        // test that search term gets set, search gets started and enough results get returned
        model->setSearchTerm(model->searchTerm() + QStringLiteral("[empty]"));

        QSignalSpy searchInProgressChanged(
            model.data(), &OCC::UnifiedSearchResultsListModel::isSearchInProgressChanged);

        QVERIFY(searchInProgressChanged.wait());

        // make sure search has started
        QCOMPARE(searchInProgressChanged.count(), 1);
        QVERIFY(model->isSearchInProgress());

        QVERIFY(searchInProgressChanged.wait());

        // make sure search has finished
        QVERIFY(!model->isSearchInProgress());

        QVERIFY(model->rowCount() == 0);
    }

    void testFetchMoreClicked()
    {
        // make sure the model is empty
        model->setSearchTerm(QStringLiteral(""));
        QVERIFY(model->rowCount() == 0);

        QSignalSpy searchInProgressChanged(
            model.data(), &OCC::UnifiedSearchResultsListModel::isSearchInProgressChanged);

        // test that search term gets set, search gets started and enough results get returned
        model->setSearchTerm(model->searchTerm() + QStringLiteral("whatever"));

        QVERIFY(searchInProgressChanged.wait());

        // make sure search has started
        QVERIFY(model->isSearchInProgress());

        QVERIFY(searchInProgressChanged.wait());

        // make sure search has finished
        QVERIFY(!model->isSearchInProgress());

        const auto numRowsInModelPrev = model->rowCount();

        // test fetch more results
        QSignalSpy currentFetchMoreInProgressProviderIdChanged(
            model.data(), &OCC::UnifiedSearchResultsListModel::currentFetchMoreInProgressProviderIdChanged);
        QSignalSpy rowsInserted(model.data(), &OCC::UnifiedSearchResultsListModel::rowsInserted);
        for (int i = 0; i < model->rowCount(); ++i) {
            const auto type = model->data(model->index(i), OCC::UnifiedSearchResultsListModel::DataRole::TypeRole);

            if (type == OCC::UnifiedSearchResult::Type::FetchMoreTrigger) {
                const auto providerId =
                    model->data(model->index(i), OCC::UnifiedSearchResultsListModel::DataRole::ProviderIdRole)
                        .toString();
                model->fetchMoreTriggerClicked(providerId);
                break;
            }
        }

        // make sure the currentFetchMoreInProgressProviderId was set back and forth and correct number fows has been inserted
        QCOMPARE(currentFetchMoreInProgressProviderIdChanged.count(), 1);

        const auto providerIdFetchMoreTriggered = model->currentFetchMoreInProgressProviderId();

        QVERIFY(!providerIdFetchMoreTriggered.isEmpty());

        QVERIFY(currentFetchMoreInProgressProviderIdChanged.wait());

        QVERIFY(model->currentFetchMoreInProgressProviderId().isEmpty());

        QCOMPARE(rowsInserted.count(), 1);

        const auto arguments = rowsInserted.takeFirst();

        QVERIFY(arguments.size() > 0);

        const auto first = arguments.at(0).toInt();
        const auto last = arguments.at(1).toInt();

        const int numInsertedExpected = last - first;

        QCOMPARE(model->rowCount() - numRowsInModelPrev, numInsertedExpected);

        // make sure the FetchMoreTrigger gets removed when no more results available
        if (!providerIdFetchMoreTriggered.isEmpty()) {
            currentFetchMoreInProgressProviderIdChanged.clear();
            rowsInserted.clear();

            QSignalSpy rowsRemoved(model.data(), &OCC::UnifiedSearchResultsListModel::rowsRemoved);

            for (int i = 0; i < 10; ++i) {
                model->fetchMoreTriggerClicked(providerIdFetchMoreTriggered);

                QVERIFY(currentFetchMoreInProgressProviderIdChanged.wait());

                if (rowsRemoved.count() > 0) {
                    break;
                }
            }
            
            QCOMPARE(rowsRemoved.count(), 1);

            bool isFetchMoreTriggerFound = false;

            for (int i = 0; i < model->rowCount(); ++i) {
                const auto type = model->data(model->index(i), OCC::UnifiedSearchResultsListModel::DataRole::TypeRole);
                const auto providerId =  model->data(model->index(i), OCC::UnifiedSearchResultsListModel::DataRole::ProviderIdRole)
                            .toString();
                if (type == OCC::UnifiedSearchResult::Type::FetchMoreTrigger
                    && providerId == providerIdFetchMoreTriggered) {
                    isFetchMoreTriggerFound = true;
                    break;
                }
            }

            QVERIFY(!isFetchMoreTriggerFound);
        }
    }

    void testSearchResultlicked()
    {
        // make sure the model is empty
        model->setSearchTerm(QStringLiteral(""));
        QVERIFY(model->rowCount() == 0);

        // test that search term gets set, search gets started and enough results get returned
        model->setSearchTerm(model->searchTerm() + QStringLiteral("discuss"));

        QSignalSpy searchInProgressChanged(
            model.data(), &OCC::UnifiedSearchResultsListModel::isSearchInProgressChanged);

        QVERIFY(searchInProgressChanged.wait());

        // make sure search has started
        QCOMPARE(searchInProgressChanged.count(), 1);
        QVERIFY(model->isSearchInProgress());

        QVERIFY(searchInProgressChanged.wait());

        // make sure search has finished and some results has been received
        QVERIFY(!model->isSearchInProgress());

        QVERIFY(model->rowCount() != 0);

        QDesktopServices::setUrlHandler("http", fakeDesktopServicesUrlHandler.data(), "resultClicked");
        QDesktopServices::setUrlHandler("https", fakeDesktopServicesUrlHandler.data(), "resultClicked");

        QSignalSpy resultClicked(fakeDesktopServicesUrlHandler.data(), &FakeDesktopServicesUrlHandler::resultClicked);
 
        //  test click on a result item
        QString urlForClickedResult;

        for (int i = 0; i < model->rowCount(); ++i) {
            const auto type = model->data(model->index(i), OCC::UnifiedSearchResultsListModel::DataRole::TypeRole);

            if (type == OCC::UnifiedSearchResult::Type::Default) {
                const auto providerId =
                    model->data(model->index(i), OCC::UnifiedSearchResultsListModel::DataRole::ProviderIdRole)
                        .toString();
                urlForClickedResult = model->data(model->index(i), OCC::UnifiedSearchResultsListModel::DataRole::ResourceUrlRole).toString();

                if (!providerId.isEmpty() && !urlForClickedResult.isEmpty()) {
                    model->resultClicked(providerId, QUrl(urlForClickedResult));
                    break;
                }
            }
        }

        QCOMPARE(resultClicked.count(), 1);

        const auto arguments = resultClicked.takeFirst();

        const auto urlOpenTriggeredViaDesktopServices = arguments.at(0).toString();

        QCOMPARE(urlOpenTriggeredViaDesktopServices, urlForClickedResult);
    }

    void testSetSearchTermResultsError()
    {
        // make sure the model is empty
        model->setSearchTerm(QStringLiteral(""));
        QVERIFY(model->rowCount() == 0);

        QSignalSpy errorStringChanged(model.data(), &OCC::UnifiedSearchResultsListModel::errorStringChanged);
        QSignalSpy searchInProgressChanged(
            model.data(), &OCC::UnifiedSearchResultsListModel::isSearchInProgressChanged);

        model->setSearchTerm(model->searchTerm() + QStringLiteral("[HTTP500]"));

        QVERIFY(searchInProgressChanged.wait());

        // make sure search has started
        QVERIFY(model->isSearchInProgress());

        QVERIFY(searchInProgressChanged.wait());

        // make sure search has finished
        QVERIFY(!model->isSearchInProgress());

        // make sure the model is empty and an error string has been set
        QVERIFY(model->rowCount() == 0);

        QVERIFY(errorStringChanged.count() > 0);

        QVERIFY(!model->errorString().isEmpty());
    }

    void cleanupTestCase()
    {
        FakeSearchResultsStorage::destroy();
    }
};

QTEST_MAIN(TestUnifiedSearchListmodel)
#include "testunifiedsearchlistmodel.moc"
