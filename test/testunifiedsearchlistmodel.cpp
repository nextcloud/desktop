/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "gui/search/unifiedsearchresultslistmodel.h"

#include "account.h"
#include "accountstate.h"
#include "syncenginetestutils.h"
#include "testhelper.h"

#include <QAbstractItemModelTester>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QMetaMethod>
#include <QSignalSpy>
#include <QStandardPaths>
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
Q_SIGNALS:
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
    void initProvidersResponse(const QString &excludedProviderId = {}, const QString &providerWithoutPersonFilter = {}, const QString &externalProviderId = {})
    {
        QList<QVariant> providersList;

        for (const auto &fakeProviderInitInfo : fakeProvidersInitInfo) {
            if (fakeProviderInitInfo._id == excludedProviderId) {
                continue;
            }
            auto filters = QVariantMap{
                {QStringLiteral("term"), QVariantMap{}},
                {QStringLiteral("since"), QVariantMap{}},
                {QStringLiteral("until"), QVariantMap{}},
                {QStringLiteral("person"), QVariantMap{}},
            };
            if (fakeProviderInitInfo._id == providerWithoutPersonFilter) {
                filters.remove(QStringLiteral("person"));
            }
            providersList.push_back(QVariantMap{
                {QStringLiteral("id"), fakeProviderInitInfo._id},
                {QStringLiteral("appId"), fakeProviderInitInfo._id},
                {QStringLiteral("name"), fakeProviderInitInfo._name},
                {QStringLiteral("order"), fakeProviderInitInfo._order},
                {QStringLiteral("filters"), filters},
                {QStringLiteral("isExternalProvider"), fakeProviderInitInfo._id == externalProviderId},
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

        if (searchTerm == QStringLiteral("[app-icons]")) {
            auto entries = QVariantList{};
            if (providerId == QStringLiteral("settings_apps")) {
                const auto titles = QStringList{
                    QStringLiteral("Dashboard"),
                    QStringLiteral("Talk"),
                    QStringLiteral("Activity"),
                    QStringLiteral("Unknown app"),
                };
                for (const auto &title : titles) {
                    entries.push_back(QVariantMap{
                        {QStringLiteral("thumbnailUrl"), QString()},
                        {QStringLiteral("title"), title},
                        {QStringLiteral("subline"), QString()},
                        {QStringLiteral("resourceUrl"), QStringLiteral("http://example.de/index.php/apps/dashboard/")},
                        {QStringLiteral("icon"), QStringLiteral("icon-arrow-right")},
                        {QStringLiteral("rounded"), false},
                    });
                }
            }
            const auto dataMap = QVariantMap{
                {QStringLiteral("name"), _searchResultsData[providerId]._name},
                {QStringLiteral("isPaginated"), false},
                {QStringLiteral("cursor"), QVariant()},
                {QStringLiteral("entries"), entries},
            };
            const auto ocsMap = QVariantMap{{QStringLiteral("meta"), _metaSuccess}, {QStringLiteral("data"), dataMap}};
            return QJsonDocument::fromVariant(QVariantMap{{QStringLiteral("ocs"), ocsMap}}).toJson(QJsonDocument::Compact);
        }

        if (searchTerm == QStringLiteral("[small-page]")) {
            auto entries = resultsForProvider(providerId, 0);
            while (entries.size() > 2) {
                entries.removeLast();
            }
            const QVariantMap dataMap = {{QStringLiteral("name"), _searchResultsData[providerId]._name},
                {QStringLiteral("isPaginated"), true}, {QStringLiteral("cursor"), cursor + entries.size()},
                {QStringLiteral("entries"), entries}};
            const QVariantMap ocsMap = {{QStringLiteral("meta"), _metaSuccess}, {QStringLiteral("data"), dataMap}};
            return QJsonDocument::fromVariant(QVariantMap{{QStringLiteral("ocs"), ocsMap}})
                .toJson(QJsonDocument::Compact);
        }

        if (searchTerm == QStringLiteral("[oversized-page]")) {
            auto entries = QList<QVariant>();
            const auto availableEntries = resultsForProvider(providerId, 0);
            if (!availableEntries.isEmpty()) {
                for (auto index = 0; index < 25; ++index) {
                    entries.push_back(availableEntries.constFirst());
                }
            }
            const QVariantMap dataMap = {{QStringLiteral("name"), _searchResultsData[providerId]._name},
                {QStringLiteral("isPaginated"), false}, {QStringLiteral("cursor"), QVariant()},
                {QStringLiteral("entries"), entries}};
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
    QScopedPointer<FakeAccountState> accountState;
    QScopedPointer<OCC::UnifiedSearchResultsListModel> model;
    QScopedPointer<QAbstractItemModelTester> modelTester;
    QList<QUrl> requestedUrls;

    QScopedPointer<FakeDesktopServicesUrlHandler> fakeDesktopServicesUrlHandler;

    static const int searchResultsReplyDelay = 100;

private Q_SLOTS:
    void initTestCase()
    {
        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);

        QStandardPaths::setTestModeEnabled(true);

        fakeQnam.reset(new FakeQNAM({}));
        account = OCC::Account::create();
        account->setCredentials(new FakeCredentials{fakeQnam.data()});
        account->setUrl(QUrl(("http://example.de")));

        accountState.reset(new FakeAccountState(account));

        fakeQnam->setOverride([this](QNetworkAccessManager::Operation op, const QNetworkRequest &req, QIODevice *device) {
            Q_UNUSED(device);
            requestedUrls.push_back(req.url());
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

    void testQmlVoidActionsAreSlots()
    {
        auto slotNames = QSet<QByteArray>{};
        const auto &metaObject = OCC::UnifiedSearchResultsListModel::staticMetaObject;
        for (auto methodIndex = metaObject.methodOffset(); methodIndex < metaObject.methodCount(); ++methodIndex) {
            const auto method = metaObject.method(methodIndex);
            if (method.methodType() == QMetaMethod::Slot) {
                slotNames.insert(method.name());
            }
        }

        const auto expectedSlots = QList<QByteArray>{
            QByteArrayLiteral("setSearchTerm"),
            QByteArrayLiteral("resultClicked"),
            QByteArrayLiteral("fetchMoreTriggerClicked"),
            QByteArrayLiteral("toggleProviderFilter"),
            QByteArrayLiteral("clearTypeFilters"),
            QByteArrayLiteral("setDatePreset"),
            QByteArrayLiteral("clearDateFilter"),
            QByteArrayLiteral("setPersonFilter"),
            QByteArrayLiteral("clearPersonFilter"),
            QByteArrayLiteral("removeFilter"),
            QByteArrayLiteral("setExternalProvidersEnabled"),
            QByteArrayLiteral("openProviderDetail"),
            QByteArrayLiteral("closeProviderDetail"),
            QByteArrayLiteral("loadMore"),
            QByteArrayLiteral("retryLoadMore"),
            QByteArrayLiteral("retryFailedProviders"),
            QByteArrayLiteral("retry"),
            QByteArrayLiteral("moveSelection"),
            QByteArrayLiteral("activateSelected"),
        };
        for (const auto &slotName : expectedSlots) {
            QVERIFY2(slotNames.contains(slotName), slotName.constData());
        }
    }

    void testSetSearchTermStartStopSearch()
    {
        // make sure the model is empty
        model->setSearchTerm(QString{});
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

        QVERIFY(!model->isSearchInProgress());

        QSignalSpy searchInProgressChanged(model.data(), &OCC::UnifiedSearchResultsListModel::isSearchInProgressChanged);
        QVERIFY(searchInProgressChanged.wait());
        QVERIFY(model->isSearchInProgress());

        // #5 test that model has stopped the search after setting empty search term
        model->setSearchTerm(QString{});
        QVERIFY(!model->isSearchInProgress());
    }

    void testSearchTermNotifiesProgressDuringProviderDiscovery()
    {
        auto qnam = std::unique_ptr<FakeQNAM>(new FakeQNAM({}));
        auto slowAccount = OCC::Account::create();
        slowAccount->setCredentials(new FakeCredentials(qnam.get()));
        slowAccount->setUrl(QUrl(QStringLiteral("http://example.de")));
        auto slowAccountState = std::make_unique<FakeAccountState>(slowAccount);
        qnam->setOverride([&](QNetworkAccessManager::Operation operation, const QNetworkRequest &request, QIODevice *) {
            return static_cast<QNetworkReply *>(
                new FakePayloadReply(operation, request, FakeSearchResultsStorage::instance()->fakeProvidersResponseJson(), 1000, qnam.get()));
        });

        OCC::UnifiedSearchResultsListModel slowModel(slowAccountState.get(), 0);
        QCoreApplication::processEvents();
        QVERIFY(!slowModel.providersReady());
        QSignalSpy progressChanged(&slowModel, &OCC::UnifiedSearchResultsListModel::isSearchInProgressChanged);

        slowModel.setSearchTerm(QStringLiteral("query"));
        QVERIFY(slowModel.isSearchInProgress());
        QCOMPARE(progressChanged.count(), 1);

        slowModel.setSearchTerm(QString());
        QVERIFY(!slowModel.isSearchInProgress());
        QCOMPARE(progressChanged.count(), 2);
    }

    void testSetSearchTermResultsFound()
    {
        // make sure the model is empty
        model->setSearchTerm(QString{});
        QVERIFY(model->rowCount() == 0);

        // test that search term gets set, search gets started and enough results get returned
        model->setSearchTerm(model->searchTerm() + QStringLiteral("discuss"));

        QTRY_VERIFY_WITH_TIMEOUT(!model->waitingForSearchTermEditEnd()
            && !model->isSearchInProgress() && model->rowCount() > 0, 2000);
    }

    void testSetSearchTermResultsNotFound()
    {
        // make sure the model is empty
        model->setSearchTerm(QString{});
        QVERIFY(model->rowCount() == 0);

        // test that search term gets set, search gets started and enough results get returned
        model->setSearchTerm(model->searchTerm() + QStringLiteral("[empty]"));

        QTRY_VERIFY_WITH_TIMEOUT(!model->waitingForSearchTermEditEnd()
            && !model->isSearchInProgress(), 2000);
        QCOMPARE(model->rowCount(), 0);
    }

    void testFetchMoreClicked()
    {
        // make sure the model is empty
        model->setSearchTerm(QString{});
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

        QString providerIdFetchMoreTriggered;
        for (auto row = 0; row < model->rowCount(); ++row) {
            if (model->data(model->index(row), OCC::UnifiedSearchResultsListModel::TypeRole)
                    == OCC::UnifiedSearchResult::Type::ProviderHeader
                && model->data(model->index(row), OCC::UnifiedSearchResultsListModel::HasOverflowRole).toBool()) {
                providerIdFetchMoreTriggered = model->data(model->index(row), OCC::UnifiedSearchResultsListModel::ProviderIdRole).toString();
                break;
            }
        }
        QVERIFY(!providerIdFetchMoreTriggered.isEmpty());
        model->openProviderDetail(providerIdFetchMoreTriggered);
        QCOMPARE(model->viewMode(), OCC::UnifiedSearchResultsListModel::ViewMode::ProviderDetail);
        const auto numRowsInModelPrev = model->rowCount();
        QStringList stableKeysBeforePaging;
        for (auto row = 0; row < numRowsInModelPrev - 1; ++row) {
            stableKeysBeforePaging.push_back(model->data(model->index(row),
                OCC::UnifiedSearchResultsListModel::StableKeyRole).toString());
        }
        QSignalSpy modelReset(model.data(), &QAbstractItemModel::modelReset);
        QSignalSpy rowsInserted(model.data(), &QAbstractItemModel::rowsInserted);
        QSignalSpy rowsRemoved(model.data(), &QAbstractItemModel::rowsRemoved);

        QSignalSpy currentFetchMoreInProgressProviderIdChanged(
            model.data(), &OCC::UnifiedSearchResultsListModel::currentFetchMoreInProgressProviderIdChanged);
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
        QVERIFY(currentFetchMoreInProgressProviderIdChanged.count() > 0);
        QTRY_VERIFY_WITH_TIMEOUT(model->currentFetchMoreInProgressProviderId().isEmpty(), 1000);
        QVERIFY(model->rowCount() > numRowsInModelPrev);
        QCOMPARE(modelReset.count(), 0);
        QVERIFY(rowsInserted.count() > 0);
        QCOMPARE(rowsRemoved.count(), 0);
        for (auto row = 0; row < stableKeysBeforePaging.size(); ++row) {
            QCOMPARE(model->data(model->index(row), OCC::UnifiedSearchResultsListModel::StableKeyRole).toString(),
                stableKeysBeforePaging[row]);
        }

        // make sure the FetchMoreTrigger gets removed when no more results available
        if (!providerIdFetchMoreTriggered.isEmpty()) {
            currentFetchMoreInProgressProviderIdChanged.clear();
            for (int i = 0; i < 10; ++i) {
                model->fetchMoreTriggerClicked(providerIdFetchMoreTriggered);
                QTRY_VERIFY_WITH_TIMEOUT(model->currentFetchMoreInProgressProviderId().isEmpty(), 1000);
            }

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

    void testSmallPaginatedFirstPageCanOpenAndLoadMore()
    {
        model->setSearchTerm(QStringLiteral("[small-page]"));
        QTRY_VERIFY_WITH_TIMEOUT(!model->waitingForSearchTermEditEnd()
            && !model->isSearchInProgress() && model->rowCount() > 0, 2000);

        auto providerId = QString();
        for (auto row = 0; row < model->rowCount(); ++row) {
            const auto index = model->index(row);
            if (model->data(index, OCC::UnifiedSearchResultsListModel::TypeRole).toInt()
                    == OCC::UnifiedSearchResult::Type::ProviderHeader
                && model->data(index, OCC::UnifiedSearchResultsListModel::HasOverflowRole).toBool()) {
                providerId = model->data(index, OCC::UnifiedSearchResultsListModel::ProviderIdRole).toString();
                break;
            }
        }
        QVERIFY(!providerId.isEmpty());

        model->openProviderDetail(providerId);
        QCOMPARE(model->viewMode(), OCC::UnifiedSearchResultsListModel::ViewMode::ProviderDetail);
        const auto rowsBeforeLoading = model->rowCount();
        QVERIFY(rowsBeforeLoading <= 3);
        model->loadMore(providerId);
        QTRY_VERIFY_WITH_TIMEOUT(model->currentFetchMoreInProgressProviderId().isEmpty(), 1000);
        QVERIFY(model->rowCount() > rowsBeforeLoading);
    }

    void testNonConformingProviderPageIsCapped()
    {
        model->setSearchTerm(QStringLiteral("[oversized-page]"));
        QTRY_VERIFY_WITH_TIMEOUT(!model->waitingForSearchTermEditEnd()
            && !model->isSearchInProgress() && model->rowCount() > 0, 2000);

        auto providerId = QString();
        for (auto row = 0; row < model->rowCount(); ++row) {
            const auto index = model->index(row);
            if (model->data(index, OCC::UnifiedSearchResultsListModel::TypeRole).toInt()
                == OCC::UnifiedSearchResult::Type::ProviderHeader) {
                providerId = model->data(index, OCC::UnifiedSearchResultsListModel::ProviderIdRole).toString();
                break;
            }
        }
        QVERIFY(!providerId.isEmpty());
        model->openProviderDetail(providerId);
        QCOMPARE(model->viewMode(), OCC::UnifiedSearchResultsListModel::ViewMode::ProviderDetail);
        QCOMPARE(model->rowCount(), 10);
    }

    void testAggregateProjectionAndKeyboardSelection()
    {
        model->setSearchTerm(QStringLiteral("projection"));
        QTRY_VERIFY_WITH_TIMEOUT(!model->waitingForSearchTermEditEnd()
            && !model->isSearchInProgress() && model->rowCount() > 0, 2000);

        QHash<QString, int> resultsByProvider;
        auto loadMoreFound = false;
        for (auto row = 0; row < model->rowCount(); ++row) {
            const auto index = model->index(row);
            const auto type = model->data(index, OCC::UnifiedSearchResultsListModel::TypeRole).toInt();
            if (type == OCC::UnifiedSearchResult::Type::Default) {
                ++resultsByProvider[model->data(index, OCC::UnifiedSearchResultsListModel::ProviderIdRole).toString()];
            } else if (type == OCC::UnifiedSearchResult::Type::FetchMoreTrigger) {
                loadMoreFound = true;
            }
        }
        for (const auto count : std::as_const(resultsByProvider)) {
            QVERIFY(count <= 3);
        }
        QVERIFY(!loadMoreFound);
        QVERIFY(model->selectedRow() >= 0);
        const auto firstSelected = model->selectedRow();
        model->moveSelection(OCC::UnifiedSearchResultsListModel::SelectionDirection::Next);
        QVERIFY(model->selectedRow() >= firstSelected);
        model->moveSelection(OCC::UnifiedSearchResultsListModel::SelectionDirection::Last);
        const auto lastSelected = model->selectedRow();
        model->moveSelection(OCC::UnifiedSearchResultsListModel::SelectionDirection::Next);
        QCOMPARE(model->selectedRow(), lastSelected);
    }

    void testAppsProviderUsesNavigationAppIcons()
    {
        const auto navigationApps = QVariantList{
            QVariantMap{{QStringLiteral("name"), QStringLiteral("Dashboard")},
                        {QStringLiteral("href"), QStringLiteral("http://example.de/index.php/apps/dashboard/")},
                        {QStringLiteral("id"), QStringLiteral("dashboard")},
                        {QStringLiteral("icon"), QStringLiteral("http://example.de/apps/dashboard/img/app.svg")}},
            QVariantMap{{QStringLiteral("name"), QStringLiteral("Talk")},
                        {QStringLiteral("href"), QStringLiteral("http://example.de/call/")},
                        {QStringLiteral("id"), QStringLiteral("spreed")},
                        {QStringLiteral("icon"), QStringLiteral("http://example.de/apps/spreed/img/app.svg")}},
            QVariantMap{{QStringLiteral("name"), QStringLiteral("Activity")},
                        {QStringLiteral("href"), QStringLiteral("http://example.de/index.php/apps/activity/")},
                        {QStringLiteral("id"), QStringLiteral("activity")},
                        {QStringLiteral("icon"), QStringLiteral("http://example.de/apps/activity/img/app.svg")}},
        };
        const auto navigationResponse = QJsonDocument::fromVariant(QVariantMap{
            {QStringLiteral("ocs"), QVariantMap{{QStringLiteral("data"), navigationApps}}},
        });
        QVERIFY(QMetaObject::invokeMethod(accountState.data(),
                                          "slotNavigationAppsFetched",
                                          Qt::DirectConnection,
                                          Q_ARG(QJsonDocument, navigationResponse),
                                          Q_ARG(int, 200)));

        model->setSearchTerm(QString());
        model->closeProviderDetail();
        model->clearTypeFilters();
        model->clearDateFilter();
        model->clearPersonFilter();
        model->setSearchTerm(QStringLiteral("[app-icons]"));
        QTRY_VERIFY_WITH_TIMEOUT(!model->waitingForSearchTermEditEnd()
            && !model->isSearchInProgress() && model->rowCount() > 0, 2000);

        model->openProviderDetail(QStringLiteral("settings_apps"));
        QCOMPARE(model->rowCount(), 4);

        const auto expectedIconUrls = QHash<QString, QString>{
            {QStringLiteral("Dashboard"), QStringLiteral("http://example.de/apps/dashboard/img/app.svg")},
            {QStringLiteral("Talk"), QStringLiteral("http://example.de/apps/spreed/img/app.svg")},
            {QStringLiteral("Activity"), QStringLiteral("http://example.de/apps/activity/img/app.svg")},
        };
        for (auto row = 0; row < model->rowCount(); ++row) {
            const auto index = model->index(row);
            const auto title = model->data(index, OCC::UnifiedSearchResultsListModel::TitleRole).toString();
            if (expectedIconUrls.contains(title)) {
                QCOMPARE(model->data(index, OCC::UnifiedSearchResultsListModel::DarkIconsRole).toString(),
                         expectedIconUrls.value(title) + QStringLiteral("/white"));
                QCOMPARE(model->data(index, OCC::UnifiedSearchResultsListModel::LightIconsRole).toString(),
                         expectedIconUrls.value(title) + QStringLiteral("/black"));
            } else if (title == QStringLiteral("Unknown app")) {
                QVERIFY(model->data(index, OCC::UnifiedSearchResultsListModel::DarkIconsRole)
                            .toString()
                            .endsWith(QStringLiteral("change.svg")));
                QVERIFY(model->data(index, OCC::UnifiedSearchResultsListModel::LightIconsRole)
                            .toString()
                            .endsWith(QStringLiteral("change.svg")));
            }
        }
    }

    void testFiltersAndRequestParameters()
    {
        model->setSearchTerm(QString());
        model->clearTypeFilters();
        model->clearDateFilter();
        model->clearPersonFilter();
        requestedUrls.clear();

        model->toggleProviderFilter(QStringLiteral("files"));
        model->setDatePreset(QStringLiteral("last7days"));
        model->setPersonFilter(QStringLiteral("ada"), QStringLiteral("Ada Lovelace"));
        model->setSearchTerm(QStringLiteral("filtered"));
        QTRY_VERIFY_WITH_TIMEOUT(!model->isSearchInProgress() && !model->waitingForSearchTermEditEnd(), 2000);

        QList<QUrl> searchRequests;
        for (const auto &url : std::as_const(requestedUrls)) {
            if (url.path().endsWith(QStringLiteral("/search"))) {
                searchRequests.push_back(url);
            }
        }
        QCOMPARE(searchRequests.size(), 1);
        QVERIFY(searchRequests.constFirst().path().contains(QStringLiteral("/files/search")));
        const QUrlQuery query(searchRequests.constFirst());
        QCOMPARE(query.queryItemValue(QStringLiteral("term")), QStringLiteral("filtered"));
        QCOMPARE(query.queryItemValue(QStringLiteral("limit")), QStringLiteral("10"));
        QCOMPARE(query.queryItemValue(QStringLiteral("person")), QStringLiteral("ada"));
        QVERIFY(!query.queryItemValue(QStringLiteral("since")).isEmpty());
        QVERIFY(!query.queryItemValue(QStringLiteral("until")).isEmpty());
        QVERIFY(!query.hasQueryItem(QStringLiteral("from")));

        model->setSearchTerm(QString());
        model->clearTypeFilters();
        model->clearDateFilter();
        model->clearPersonFilter();
    }

    void testConnectedServicesActionIsHiddenWithProviderFilter()
    {
        model->setSearchTerm(QString());
        model->clearTypeFilters();

        accountState->setStateForTesting(OCC::AccountState::Disconnected);
        QVERIFY(QMetaObject::invokeMethod(accountState.data(), "isConnectedChanged", Qt::DirectConnection));
        FakeSearchResultsStorage::instance()->initProvidersResponse({}, {}, QStringLiteral("mail"));
        accountState->setStateForTesting(OCC::AccountState::Connected);
        QVERIFY(QMetaObject::invokeMethod(accountState.data(), "isConnectedChanged", Qt::DirectConnection));
        QTRY_VERIFY_WITH_TIMEOUT(model->providersReady(), 1000);

        model->setSearchTerm(QStringLiteral("connected services"));
        QTRY_VERIFY_WITH_TIMEOUT(!model->isSearchInProgress() && !model->waitingForSearchTermEditEnd(), 2000);
        QVERIFY(model->showConnectedServicesAction());

        model->toggleProviderFilter(QStringLiteral("files"));
        QVERIFY(!model->showConnectedServicesAction());

        model->setSearchTerm(QString());
        model->clearTypeFilters();
        accountState->setStateForTesting(OCC::AccountState::Disconnected);
        QVERIFY(QMetaObject::invokeMethod(accountState.data(), "isConnectedChanged", Qt::DirectConnection));
        FakeSearchResultsStorage::instance()->initProvidersResponse();
        accountState->setStateForTesting(OCC::AccountState::Connected);
        QVERIFY(QMetaObject::invokeMethod(accountState.data(), "isConnectedChanged", Qt::DirectConnection));
        QTRY_VERIFY_WITH_TIMEOUT(model->providersReady(), 1000);
    }

    void testSearchResultlicked()
    {
        // make sure the model is empty
        model->setSearchTerm(QString{});
        QVERIFY(model->rowCount() == 0);

        // test that search term gets set, search gets started and enough results get returned
        model->setSearchTerm(model->searchTerm() + QStringLiteral("discuss"));

        QTRY_VERIFY_WITH_TIMEOUT(!model->waitingForSearchTermEditEnd()
            && !model->isSearchInProgress() && model->rowCount() > 0, 2000);

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
        model->setSearchTerm(QString{});
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

    void testSearchStatePlaceholderWhenNoSearchTerm()
    {
        model->setSearchTerm(QString{});
        QVERIFY(model->rowCount() == 0);

        QVERIFY(!model->hasSearchTerm());
        QVERIFY(!model->hasSearchError());
        QCOMPARE(model->searchState(), OCC::UnifiedSearchResultsListModel::SearchState::Placeholder);
    }

    void testSearchStateSkeletonWhileSearching()
    {
        model->setSearchTerm(QString{});
        QVERIFY(model->rowCount() == 0);

        QSignalSpy searchStateChanged(model.data(), &OCC::UnifiedSearchResultsListModel::searchStateChanged);

        model->setSearchTerm(QStringLiteral("dis"));

        // waiting for the edit timer to fire, nothing fetched yet
        QVERIFY(model->waitingForSearchTermEditEnd());
        QVERIFY(!model->isSearchInProgress());
        QVERIFY(model->hasSearchTerm());
        QCOMPARE(model->searchState(), OCC::UnifiedSearchResultsListModel::SearchState::Skeleton);
        QVERIFY(searchStateChanged.count() > 0);

        QSignalSpy searchInProgressChanged(
            model.data(), &OCC::UnifiedSearchResultsListModel::isSearchInProgressChanged);

        // search started but results have not arrived yet
        QVERIFY(searchInProgressChanged.wait());
        QVERIFY(model->isSearchInProgress());
        QVERIFY(model->rowCount() == 0);
        QCOMPARE(model->searchState(), OCC::UnifiedSearchResultsListModel::SearchState::Skeleton);

        model->setSearchTerm(QString{});
    }

    void testSearchStateResultsWhenResultsFound()
    {
        model->setSearchTerm(QString{});
        QVERIFY(model->rowCount() == 0);

        QSignalSpy searchStateChanged(model.data(), &OCC::UnifiedSearchResultsListModel::searchStateChanged);
        QSignalSpy searchInProgressChanged(
            model.data(), &OCC::UnifiedSearchResultsListModel::isSearchInProgressChanged);

        model->setSearchTerm(QStringLiteral("discuss"));

        QVERIFY(searchInProgressChanged.wait());
        QVERIFY(model->isSearchInProgress());

        QVERIFY(searchInProgressChanged.wait());
        QVERIFY(!model->isSearchInProgress());

        QVERIFY(model->rowCount() > 0);
        QVERIFY(!model->hasSearchError());
        QVERIFY(!model->isFetchMoreInProgress());
        QCOMPARE(model->searchState(), OCC::UnifiedSearchResultsListModel::SearchState::Results);
        QVERIFY(searchStateChanged.count() > 0);

        model->setSearchTerm(QString{});
    }

    void testSearchStateNothingFoundWhenNoResults()
    {
        model->setSearchTerm(QString{});
        QVERIFY(model->rowCount() == 0);

        QSignalSpy searchInProgressChanged(
            model.data(), &OCC::UnifiedSearchResultsListModel::isSearchInProgressChanged);

        model->setSearchTerm(QStringLiteral("[empty]"));

        QVERIFY(searchInProgressChanged.wait());
        QVERIFY(model->isSearchInProgress());

        QVERIFY(searchInProgressChanged.wait());
        QVERIFY(!model->isSearchInProgress());

        QVERIFY(model->rowCount() == 0);
        QVERIFY(model->hasSearchTerm());
        QVERIFY(!model->hasSearchError());
        QCOMPARE(model->searchState(), OCC::UnifiedSearchResultsListModel::SearchState::NothingFound);

        model->setSearchTerm(QString{});
    }

    void testSearchStateSearchErrorWhenSearchFails()
    {
        model->setSearchTerm(QString{});
        QVERIFY(model->rowCount() == 0);

        QSignalSpy searchInProgressChanged(
            model.data(), &OCC::UnifiedSearchResultsListModel::isSearchInProgressChanged);

        model->setSearchTerm(QStringLiteral("[HTTP500]"));

        QVERIFY(searchInProgressChanged.wait());
        QVERIFY(model->isSearchInProgress());

        QVERIFY(searchInProgressChanged.wait());
        QVERIFY(!model->isSearchInProgress());

        QVERIFY(model->rowCount() == 0);
        QVERIFY(model->hasSearchError());
        QCOMPARE(model->searchState(), OCC::UnifiedSearchResultsListModel::SearchState::SearchError);

        model->setSearchTerm(QString{});
    }

    void testProviderDetailPreservesPartialMatchState()
    {
        model->setSearchTerm(QString());
        model->closeProviderDetail();
        model->clearTypeFilters();
        model->clearDateFilter();
        model->clearPersonFilter();

        accountState->setStateForTesting(OCC::AccountState::Disconnected);
        QVERIFY(QMetaObject::invokeMethod(accountState.data(), "isConnectedChanged", Qt::DirectConnection));
        FakeSearchResultsStorage::instance()->initProvidersResponse({}, QStringLiteral("mail"));
        accountState->setStateForTesting(OCC::AccountState::Connected);
        QVERIFY(QMetaObject::invokeMethod(accountState.data(), "isConnectedChanged", Qt::DirectConnection));
        QTRY_VERIFY_WITH_TIMEOUT(model->providersReady(), 1000);
        FakeSearchResultsStorage::instance()->initProvidersResponse();

        model->setPersonFilter(QStringLiteral("ada"), QStringLiteral("Ada Lovelace"));
        model->setSearchTerm(QStringLiteral("partial detail"));
        QTRY_VERIFY_WITH_TIMEOUT(!model->isSearchInProgress() && !model->waitingForSearchTermEditEnd(), 2000);

        model->openProviderDetail(QStringLiteral("mail"));
        QCOMPARE(model->viewMode(), OCC::UnifiedSearchResultsListModel::ViewMode::ProviderDetail);
        auto resultCount = 0;
        for (auto row = 0; row < model->rowCount(); ++row) {
            const auto index = model->index(row);
            if (model->data(index, OCC::UnifiedSearchResultsListModel::TypeRole).toInt() != OCC::UnifiedSearchResult::Type::Default) {
                continue;
            }
            ++resultCount;
            QVERIFY(model->data(index, OCC::UnifiedSearchResultsListModel::PartialMatchRole).toBool());
        }
        QVERIFY(resultCount > 0);

        const auto rowsBeforePaging = model->rowCount();
        model->loadMore(QStringLiteral("mail"));
        QTRY_VERIFY_WITH_TIMEOUT(model->currentFetchMoreInProgressProviderId().isEmpty(), 1000);
        QVERIFY(model->rowCount() > rowsBeforePaging);
        for (auto row = 0; row < model->rowCount(); ++row) {
            const auto index = model->index(row);
            if (model->data(index, OCC::UnifiedSearchResultsListModel::TypeRole).toInt() == OCC::UnifiedSearchResult::Type::Default) {
                QVERIFY(model->data(index, OCC::UnifiedSearchResultsListModel::PartialMatchRole).toBool());
            }
        }

        model->setSearchTerm(QString());
        model->closeProviderDetail();
        model->clearPersonFilter();
        accountState->setStateForTesting(OCC::AccountState::Disconnected);
        QVERIFY(QMetaObject::invokeMethod(accountState.data(), "isConnectedChanged", Qt::DirectConnection));
        accountState->setStateForTesting(OCC::AccountState::Connected);
        QVERIFY(QMetaObject::invokeMethod(accountState.data(), "isConnectedChanged", Qt::DirectConnection));
        QTRY_VERIFY_WITH_TIMEOUT(model->providersReady(), 1000);
    }

    void testDisconnectAndRediscoveryResetProviderState()
    {
        model->setSearchTerm(QString());
        model->clearTypeFilters();
        QVERIFY(model->providersReady());
        model->toggleProviderFilter(QStringLiteral("files"));
        QCOMPARE(model->activeFilters().size(), 1);
        QSignalSpy providersReadyChanged(model.data(), &OCC::UnifiedSearchResultsListModel::providersReadyChanged);

        accountState->setStateForTesting(OCC::AccountState::Disconnected);
        QVERIFY(QMetaObject::invokeMethod(accountState.data(), "isConnectedChanged", Qt::DirectConnection));
        QCOMPARE(model->providersReady(), false);
        QVERIFY(providersReadyChanged.count() > 0);

        FakeSearchResultsStorage::instance()->initProvidersResponse(QStringLiteral("files"));
        accountState->setStateForTesting(OCC::AccountState::Connected);
        QVERIFY(QMetaObject::invokeMethod(accountState.data(), "isConnectedChanged", Qt::DirectConnection));
        QTRY_VERIFY_WITH_TIMEOUT(model->providersReady(), 1000);
        QCOMPARE(model->activeFilters().size(), 0);
        for (const auto &providerValue : model->providers()) {
            QVERIFY(providerValue.toMap().value(QStringLiteral("id")).toString() != QStringLiteral("files"));
        }

        FakeSearchResultsStorage::instance()->initProvidersResponse();
    }

    void cleanupTestCase()
    {
        FakeSearchResultsStorage::destroy();
    }
};

QTEST_MAIN(TestUnifiedSearchListmodel)
#include "testunifiedsearchlistmodel.moc"
