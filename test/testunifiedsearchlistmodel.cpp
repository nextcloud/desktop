/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "gui/tray/unifiedsearchresultslistmodel.h"

#include "account.h"
#include "accountstate.h"
#include "folderman.h"
#include "syncenginetestutils.h"
#include "testhelper.h"

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
    void resultClickedBrowser(const QUrl &url);
    void resultClickedLocalFile(const QUrl &url);
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

class TestUnifiedSearchListModel : public QObject
{
    Q_OBJECT

    std::unique_ptr<OCC::FolderMan> _folderMan;

public:
    TestUnifiedSearchListModel() = default;

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
        _folderMan.reset(new OCC::FolderMan{});

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

    void testSearchResultClicked()
    {
        QDesktopServices::setUrlHandler("http", fakeDesktopServicesUrlHandler.data(), "resultClickedBrowser");
        QDesktopServices::setUrlHandler("https", fakeDesktopServicesUrlHandler.data(), "resultClickedBrowser");
        QDesktopServices::setUrlHandler("file", fakeDesktopServicesUrlHandler.data(), "resultClickedLocalFile");

        QSignalSpy resultClickedBrowser(fakeDesktopServicesUrlHandler.data(), &FakeDesktopServicesUrlHandler::resultClickedBrowser);
        QSignalSpy resultClickedLocalFile(fakeDesktopServicesUrlHandler.data(), &FakeDesktopServicesUrlHandler::resultClickedLocalFile);

        // Setup folder structure for further tests
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        const auto localFolderPrefix = "file:///" + fakeFolder.localPath();
        auto folderDef = folderDefinition(fakeFolder.localPath());
        folderDef.targetPath = "";
        const auto folder = OCC::FolderMan::instance()->addFolder(accountState.data(), folderDef);
        QVERIFY(folder);

        // Provider IDs which are not files, will be opened in the browser
        const auto providerIdTestProviderId = "settings_apps";
        const auto sublineTestProviderId = "John Doe in Apps";
        const auto titleTestProviderId = " We had a discussion about Apps already. But, let's have a follow up tomorrow afternoon.";
        const auto resourceUrlTestProviderId = "http://example.de/call/abcde12345#message_12345";
        model->resultClicked(providerIdTestProviderId,
            QUrl(resourceUrlTestProviderId), sublineTestProviderId, titleTestProviderId);

        QCOMPARE(resultClickedBrowser.count(), 1);
        QCOMPARE(resultClickedLocalFile.count(), 0);

        auto arguments = resultClickedBrowser.takeFirst();
        auto urlOpenTriggeredViaDesktopServices = arguments.at(0).toString();
        QCOMPARE(urlOpenTriggeredViaDesktopServices, resourceUrlTestProviderId);

        resultClickedBrowser.clear();
        resultClickedLocalFile.clear();

        // Nextcloud 20 opens in browser if the folder is not available locally
        const auto providerIdTestNextcloud20Browser = "file";
        const auto sublineTestNextcloud20Browser = "in folder/nested/searched_file.cpp";
        const auto titleTestNextcloud20Browser = "searched_file.cpp";
        const auto resourceUrlTestNextcloud20Browser = "http://example.de/files/?dir=folder/nested&scrollto=searched_file.cpp";
        model->resultClicked(providerIdTestNextcloud20Browser,
            QUrl(resourceUrlTestNextcloud20Browser), sublineTestNextcloud20Browser, titleTestNextcloud20Browser);

        QCOMPARE(resultClickedBrowser.count(), 1);
        QCOMPARE(resultClickedLocalFile.count(), 0);

        arguments = resultClickedBrowser.takeFirst();
        urlOpenTriggeredViaDesktopServices = arguments.at(0).toString();
        QCOMPARE(urlOpenTriggeredViaDesktopServices, resourceUrlTestNextcloud20Browser);

        resultClickedBrowser.clear();
        resultClickedLocalFile.clear();

        // Nextcloud versions above 20 opens in browser if the folder is not available locally
        const auto providerIdTestNextcloudAbove20Browser = "file";
        const auto sublineTestNextcloudAbove20Browser = "in folder/nested";
        const auto titleTestNextcloudAbove20Browser = "searched_file.cpp";
        const auto resourceUrlTestNextcloudAbove20Browser = "http://example.de/index.php/f/123";
        model->resultClicked(providerIdTestNextcloudAbove20Browser,
            QUrl(resourceUrlTestNextcloudAbove20Browser), sublineTestNextcloudAbove20Browser,
            titleTestNextcloudAbove20Browser);

        QCOMPARE(resultClickedBrowser.count(), 1);
        QCOMPARE(resultClickedLocalFile.count(), 0);

        arguments = resultClickedBrowser.takeFirst();
        urlOpenTriggeredViaDesktopServices = arguments.at(0).toString();
        QCOMPARE(urlOpenTriggeredViaDesktopServices, resourceUrlTestNextcloudAbove20Browser);

        resultClickedBrowser.clear();
        resultClickedLocalFile.clear();

        // Nextcloud 20 opens in local files if the file is available locally
        const auto providerIdTestNextcloud20LocalFile = "file";
        const auto sublineTestNextcloud20LocalFile = "in B/b1";
        const auto titleTestNextcloud20LocalFile = "b1";
        const auto resourceUrlTestNextcloud20LocalFile = "http://example.de/files/?dir=/B&scrollto=b1";
        const auto expectedFileUrlNextcloud20 = localFolderPrefix + "B/b1";
        model->resultClicked(providerIdTestNextcloud20LocalFile,
            QUrl(resourceUrlTestNextcloud20LocalFile), sublineTestNextcloud20LocalFile, titleTestNextcloud20LocalFile);

        QCOMPARE(resultClickedBrowser.count(), 0);
        QCOMPARE(resultClickedLocalFile.count(), 1);

        arguments = resultClickedLocalFile.takeFirst();
        urlOpenTriggeredViaDesktopServices = arguments.at(0).toString();
        QCOMPARE(urlOpenTriggeredViaDesktopServices, expectedFileUrlNextcloud20);

        resultClickedBrowser.clear();
        resultClickedLocalFile.clear();

        // Nextcloud 20 opens in local files if the file is available locally
        // The rood directory has a special syntax
        const auto providerIdTestNextcloud20LocalFileRoot = "file";
        const auto sublineTestNextcloud20LocalFileRoot = "in B";
        const auto titleTestNextcloud20LocalFileRoot = "/B";
        const auto resourceUrlTestNextcloud20LocalFileRoot = "http://example.de/files/?dir=/&scrollto=B";
        const auto expectedFileUrlNextcloud20Root = localFolderPrefix + "B";
        model->resultClicked(providerIdTestNextcloud20LocalFileRoot,
            QUrl(resourceUrlTestNextcloud20LocalFileRoot), sublineTestNextcloud20LocalFileRoot, titleTestNextcloud20LocalFileRoot);

        QCOMPARE(resultClickedBrowser.count(), 0);
        QCOMPARE(resultClickedLocalFile.count(), 1);

        arguments = resultClickedLocalFile.takeFirst();
        urlOpenTriggeredViaDesktopServices = arguments.at(0).toString();
        QCOMPARE(urlOpenTriggeredViaDesktopServices, expectedFileUrlNextcloud20Root);

        resultClickedBrowser.clear();
        resultClickedLocalFile.clear();

        // Nextcloud versions above 20 opens in local file if the file is available locally
        const auto providerIdTestNextcloudAbove20LocalFile = "file";
        const auto sublineTestNextcloudAbove20LocalFile = "in A";
        const auto titleTestNextcloudAbove20LocalFile = "a1";
        const auto resourceUrlTestNextcloudAbove20LocalFile = "http://example.de/index.php/f/456";
        const auto expectedFileUrlNextcloudAbove20 = localFolderPrefix + "A/a1";
        model->resultClicked(providerIdTestNextcloudAbove20LocalFile,
            QUrl(resourceUrlTestNextcloudAbove20LocalFile), sublineTestNextcloudAbove20LocalFile,
            titleTestNextcloudAbove20LocalFile);

        QCOMPARE(resultClickedBrowser.count(), 0);
        QCOMPARE(resultClickedLocalFile.count(), 1);

        arguments = resultClickedLocalFile.takeFirst();
        urlOpenTriggeredViaDesktopServices = arguments.at(0).toString();
        QCOMPARE(urlOpenTriggeredViaDesktopServices, expectedFileUrlNextcloudAbove20);

        resultClickedBrowser.clear();
        resultClickedLocalFile.clear();

        // Nextcloud versions above 20 opens in local folder if the file is available locally
        // In this case the local folder is opened in the root directory
        const auto providerIdTestNextcloudAbove20LocalFileRoot = "file";
        const auto sublineTestNextcloudAbove20LocalFileRoot = "";
        const auto titleTestNextcloudAbove20LocalFileRoot = "A";
        const auto resourceUrlTestNextcloudAbove20LocalFileRoot = "http://example.de/index.php/f/789";
        const auto expectedFileUrlNextcloudAbove20Root = localFolderPrefix + "A";
        model->resultClicked(providerIdTestNextcloudAbove20LocalFileRoot,
            QUrl(resourceUrlTestNextcloudAbove20LocalFileRoot), sublineTestNextcloudAbove20LocalFileRoot,
            titleTestNextcloudAbove20LocalFileRoot);

        QCOMPARE(resultClickedBrowser.count(), 0);
        QCOMPARE(resultClickedLocalFile.count(), 1);

        arguments = resultClickedLocalFile.takeFirst();
        urlOpenTriggeredViaDesktopServices = arguments.at(0).toString();
        QCOMPARE(urlOpenTriggeredViaDesktopServices, expectedFileUrlNextcloudAbove20Root);

        resultClickedBrowser.clear();
        resultClickedLocalFile.clear();

        // Accountptr is invalid
        const auto prevAccountState = accountState.data();
        model.reset(new OCC::UnifiedSearchResultsListModel(nullptr));
        modelTester.reset(new QAbstractItemModelTester(model.data()));
        const auto providerIdTestNullptr = "file";
        const auto sublineTestNullptr = "";
        const auto titleTestNullptr = "A";
        const auto resourceUrlTestNullptr = "http://example.de/index.php/f/789";
        model->resultClicked(providerIdTestNullptr,
            QUrl(resourceUrlTestNullptr), sublineTestNullptr, titleTestNullptr);

        QCOMPARE(resultClickedBrowser.count(), 0);
        QCOMPARE(resultClickedLocalFile.count(), 0);

        resultClickedBrowser.clear();
        resultClickedLocalFile.clear();

        model.reset(new OCC::UnifiedSearchResultsListModel(prevAccountState));
        modelTester.reset(new QAbstractItemModelTester(model.data()));
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

QTEST_MAIN(TestUnifiedSearchListModel)
#include "testunifiedsearchlistmodel.moc"
