/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#include <QtTest/QtTest>
#include <QDesktopServices>

#include "libsync/creds/oauth.h"
#include "testutils/syncenginetestutils.h"
#include "theme.h"
#include "common/asserts.h"

using namespace std::chrono_literals;
using namespace OCC;

class DesktopServiceHook : public QObject
{
    Q_OBJECT
signals:
    void hooked(const QUrl &);

public:
    DesktopServiceHook() { QDesktopServices::setUrlHandler(QStringLiteral("oauthtest"), this, "hooked"); }
    ~DesktopServiceHook() { QDesktopServices::unsetUrlHandler(QStringLiteral("oauthtest")); }
};

static const QUrl sOAuthTestServer(QStringLiteral("oauthtest://someserver/owncloud"));


class FakePostReply : public QNetworkReply
{
    Q_OBJECT
public:
    std::unique_ptr<QIODevice> payload;
    bool aborted = false;

    FakePostReply(QNetworkAccessManager::Operation op, const QNetworkRequest &request,
                  std::unique_ptr<QIODevice> payload_, QObject *parent)
        : QNetworkReply{parent}, payload{std::move(payload_)}
    {
        setRequest(request);
        setUrl(request.url());
        setOperation(op);
        open(QIODevice::ReadOnly);
        payload->open(QIODevice::ReadOnly);
        QMetaObject::invokeMethod(this, "respond", Qt::QueuedConnection);
    }

    Q_INVOKABLE virtual void respond() {
        if (aborted) {
            setError(OperationCanceledError, QStringLiteral("Operation Canceled"));
            emit metaDataChanged();
            emit finished();
            return;
        }
        setHeader(QNetworkRequest::ContentLengthHeader, payload->size());
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);
        emit metaDataChanged();
        if (bytesAvailable())
            emit readyRead();
        emit finished();
    }

    void abort() override {
        aborted = true;
    }
    qint64 bytesAvailable() const override {
        if (aborted)
            return 0;
        return payload->bytesAvailable();
    }

    qint64 readData(char *data, qint64 maxlen) override {
        return payload->read(data, maxlen);
    }
};

// Reply with a small delay
class SlowFakePostReply : public FakePostReply {
    Q_OBJECT
public:
    using FakePostReply::FakePostReply;
    void respond() override {
        // override of FakePostReply::respond, will call the real one with a delay.
        QTimer::singleShot(100ms, this, [this] { this->FakePostReply::respond(); });
    }
};


class OAuthTestCase : public QObject
{
    Q_OBJECT
    DesktopServiceHook desktopServiceHook;

protected:
    QString _expectedClientId = Theme::instance()->oauthClientId();

public:
    enum State { StartState,
        StatusPhpState,
        BrowserOpened,
        TokenAsked,
        UserInfoFetched,
        CustomState } state = StartState;
    Q_ENUM(State);

    // for oauth2 we use localhost, for oidc we use 127.0.0.1
    QString localHost = QStringLiteral("localhost");
    bool replyToBrowserOk = false;
    bool gotAuthOk = false;
    virtual bool done() const { return replyToBrowserOk && gotAuthOk; }

    FakeAM *fakeAm = nullptr;
    QNetworkAccessManager realQNAM;
    QPointer<QNetworkReply> browserReply = nullptr;
    QString code = QString::fromUtf8(generateEtag());
    OCC::AccountPtr account;

    QScopedPointer<OAuth> oauth;

    virtual void test() {
        fakeAm = new FakeAM({});
        account = Account::create(QUuid::createUuid());
        account->setUrl(sOAuthTestServer);
        // the account seizes ownership over the qnam in account->setCredentials(...) by keeping a shared pointer on it
        // therefore, we should never call fakeAm->setThis(...)
        account->setCredentials(new FakeCredentials { fakeAm });
        fakeAm->setOverride([this](QNetworkAccessManager::Operation op, const QNetworkRequest &req, QIODevice *device) {
            if (req.url().path().endsWith(QLatin1String(".well-known/openid-configuration"))) {
                return this->wellKnownReply(op, req);
            } else if (req.url().path().endsWith(QLatin1String("status.php"))) {
                return this->statusPhpReply(op, req);
            } else if (req.url().path().endsWith(QLatin1String("ocs/v2.php/cloud/user")) && req.url().query() == QLatin1String("format=json")) {
                return this->userInfoReply(op, req);
            } else if (req.url().path().endsWith(QLatin1String("clients-registrations"))) {
                return this->clientRegistrationReply(op, req);
            }
            OC_ASSERT(device);
            OC_ASSERT(device && device->bytesAvailable() > 0); // OAuth2 always sends around POST data.
            return this->tokenReply(op, req);
        });

        QObject::connect(&desktopServiceHook, &DesktopServiceHook::hooked,
                         this, &OAuthTestCase::openBrowserHook);

        oauth.reset(new AccountBasedOAuth(account, this));
        QObject::connect(oauth.data(), &OAuth::result, this, &OAuthTestCase::oauthResult);
        oauth->startAuthentication();

        QSignalSpy spy(oauth.data(), &OCC::OAuth::authorisationLinkChanged);
        if (spy.wait()) {
            oauth->openBrowser();
        }

        QTRY_VERIFY(done());
    }

    virtual void openBrowserHook(const QUrl &url) {
        QCOMPARE(state, StatusPhpState);
        state = BrowserOpened;
        QCOMPARE(url.path(), sOAuthTestServer.path() + QStringLiteral("/index.php/apps/oauth2/authorize"));
        QVERIFY(url.toString().startsWith(sOAuthTestServer.toString()));
        QUrlQuery query(url);
        QCOMPARE(query.queryItemValue(QStringLiteral("response_type")), QLatin1String("code"));
        QCOMPARE(query.queryItemValue(QStringLiteral("client_id")), _expectedClientId);
        QUrl redirectUri(query.queryItemValue(QStringLiteral("redirect_uri")));
        QCOMPARE(redirectUri.host(), localHost);
        redirectUri.setQuery(QStringLiteral("code=%1&state=%2").arg(code, query.queryItemValue(QStringLiteral("state"))));
        createBrowserReply(QNetworkRequest(redirectUri));
    }

    virtual QNetworkReply *createBrowserReply(const QNetworkRequest &request) {
        auto r = request;
        // don't follow the redirect to owncloud://success
        r.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
        browserReply = realQNAM.get(r);
        QObject::connect(browserReply, &QNetworkReply::finished, this, &OAuthTestCase::browserReplyFinished);
        return browserReply;
    }

    virtual void browserReplyFinished() {
        QCOMPARE(sender(), browserReply.data());
        QCOMPARE(state, UserInfoFetched);
        browserReply->deleteLater();
        QCOMPARE(QNetworkReply::NoError, browserReply->error());
        QCOMPARE(browserReply->rawHeader("Location"), QByteArray("owncloud://success"));
        replyToBrowserOk = true;
    }

    virtual QNetworkReply *tokenReply(QNetworkAccessManager::Operation op, const QNetworkRequest &req)
    {
        OC_ASSERT(state == BrowserOpened);
        state = TokenAsked;
        OC_ASSERT(op == QNetworkAccessManager::PostOperation);
        OC_ASSERT(req.url().toString().startsWith(sOAuthTestServer.toString()));
        OC_ASSERT(req.url().path() == sOAuthTestServer.path() + QStringLiteral("/index.php/apps/oauth2/api/v1/token"));
        auto payload = std::make_unique<QBuffer>();
        payload->setData(tokenReplyPayload());
        return new FakePostReply(op, req, std::move(payload), fakeAm);
    }

    virtual QNetworkReply *statusPhpReply(QNetworkAccessManager::Operation op, const QNetworkRequest &req)
    {
        OC_ASSERT(state == StartState);
        state = StatusPhpState;
        OC_ASSERT(op == QNetworkAccessManager::GetOperation);
        OC_ASSERT(req.url().toString().startsWith(sOAuthTestServer.toString()));
        OC_ASSERT(req.url().path() == sOAuthTestServer.path() + QStringLiteral("/status.php"));
        auto payload = std::make_unique<QBuffer>();
        payload->setData(statusPhpPayload());
        return new FakePostReply(op, req, std::move(payload), fakeAm);
    }

    virtual QNetworkReply *userInfoReply(QNetworkAccessManager::Operation op, const QNetworkRequest &req)
    {
        OC_ASSERT(state == TokenAsked);
        state = UserInfoFetched;
        OC_ASSERT(op == QNetworkAccessManager::GetOperation);
        OC_ASSERT(req.url().toString().startsWith(sOAuthTestServer.toString()));
        OC_ASSERT(req.url().path() == sOAuthTestServer.path() + QStringLiteral("/ocs/v2.php/cloud/user"));
        OC_ASSERT(req.url().query() == QStringLiteral("format=json"));
        auto payload = std::make_unique<QBuffer>();
        payload->setData(userInfoPayload());
        return new FakePostReply(op, req, std::move(payload), fakeAm);
    }

    virtual QNetworkReply *wellKnownReply(QNetworkAccessManager::Operation op, const QNetworkRequest &req)
    {
        return new FakeErrorReply(op, req, fakeAm, 404);
    }

    virtual QNetworkReply *clientRegistrationReply(QNetworkAccessManager::Operation op, const QNetworkRequest &req)
    {
        return new FakeErrorReply(op, req, fakeAm, 404, {});
    }

    virtual QByteArray tokenReplyPayload() const {
        // the dummy server provides the user admin
        QJsonDocument jsondata(QJsonObject{{QStringLiteral("access_token"), QStringLiteral("123")}, {QStringLiteral("refresh_token"), QStringLiteral("456")},
            {QStringLiteral("message_url"), QStringLiteral("owncloud://success")}, {QStringLiteral("user_id"), QStringLiteral("admin")},
            {QStringLiteral("token_type"), QStringLiteral("Bearer")}});
        return jsondata.toJson();
    }

    virtual QByteArray statusPhpPayload() const
    {
        QJsonDocument jsondata(
            QJsonObject{{QStringLiteral("installed"), true}, {QStringLiteral("maintenance"), false}, {QStringLiteral("needsDbUpgrade"), false},
                {QStringLiteral("version"), QStringLiteral("10.5.0.10")}, {QStringLiteral("versionstring"), QStringLiteral("10.5.0")},
                {QStringLiteral("edition"), QStringLiteral("Enterprise")}, {QStringLiteral("productname"), QStringLiteral("ownCloud")}});
        return jsondata.toJson();
    }

    virtual QByteArray userInfoPayload() const
    {
        // the dummy server provides the user admin
        // we don't provide "meta" at the moment, since it is not used
        QJsonDocument jsonData(QJsonObject{{QStringLiteral("ocs"),
            QJsonObject{{QStringLiteral("data"),
                QJsonObject{
                    {QStringLiteral("display-name"), QStringLiteral("Admin")},
                    {QStringLiteral("id"), QStringLiteral("admin")},
                    {QStringLiteral("email"), QStringLiteral("admin@admin.admin")},

                }}}}});
        return jsonData.toJson();
    }

    virtual void oauthResult(OAuth::Result result, const QString &token, const QString &refreshToken)
    {
        QCOMPARE(result, OAuth::LoggedIn);
        QCOMPARE(state, UserInfoFetched);
        QCOMPARE(token, QStringLiteral("123"));
        QCOMPARE(refreshToken, QStringLiteral("456"));
        gotAuthOk = true;
    }
};

class TestOAuth: public QObject
{
    Q_OBJECT

private slots:
    void testBasic()
    {
        OAuthTestCase test;
        test.test();
    }


    void testWrongUser()
    {
        struct Test : OAuthTestCase {
            QByteArray tokenReplyPayload() const override {
                // the dummy server provides the user admin
                QJsonDocument jsondata(QJsonObject{{QStringLiteral("access_token"), QStringLiteral("123")},
                    {QStringLiteral("refresh_token"), QStringLiteral("456")}, {QStringLiteral("message_url"), QStringLiteral("owncloud://success")},
                    {QStringLiteral("user_id"), QStringLiteral("wrong_user")}, {QStringLiteral("token_type"), QStringLiteral("Bearer")}});
                return jsondata.toJson();
            }

            void browserReplyFinished() override {
                QCOMPARE(sender(), browserReply.data());
                QCOMPARE(state, TokenAsked);
                browserReply->deleteLater();
                QCOMPARE(QNetworkReply::AuthenticationRequiredError, browserReply->error());
            }

            bool done() const override{
                return true;
            }
        };
        Test test;
        test.test();
    }

    // Test for https://github.com/owncloud/client/pull/6057
    void testCloseBrowserDontCrash()
    {
        struct Test : OAuthTestCase {
            QNetworkReply *tokenReply(QNetworkAccessManager::Operation op, const QNetworkRequest & req) override
            {
                OC_ASSERT(browserReply);
                // simulate the fact that the browser is closing the connection
                browserReply->abort();
                // don't process network events, as it messes up the execution order and
                // causes an Qt internal crash
                QCoreApplication::processEvents(QEventLoop::ExcludeSocketNotifiers);

                OC_ASSERT(state == BrowserOpened);
                state = TokenAsked;

                auto payload = std::make_unique<QBuffer>();
                payload->setData(tokenReplyPayload());
                return new SlowFakePostReply(op, req, std::move(payload), fakeAm);
            }

            void browserReplyFinished() override
            {
                QCOMPARE(sender(), browserReply.data());
                QCOMPARE(browserReply->error(), QNetworkReply::OperationCanceledError);
                replyToBrowserOk = true;
            }
        } test;
        test.test();
    }

    void testRandomConnections()
    {
        // Test that we can send random garbage to the litening socket and it does not prevent the connection
        struct Test : OAuthTestCase {
            QNetworkReply *createBrowserReply(const QNetworkRequest &request) override {
                QTimer::singleShot(0, this, [this, request] {
                    auto port = request.url().port();
                    state = CustomState;
                    const QVector<QByteArray> payloads = {
                        "GET FOFOFO HTTP 1/1\n\n",
                        "GET /?code=invalie HTTP 1/1\n\n",
                        "GET /?code=xxxxx&bar=fff",
                        QByteArray("\0\0\0", 3),
                        QByteArray("GET \0\0\0 \n\n\n\n\n\0", 14),
                        QByteArray("GET /?code=éléphant\xa5 HTTP\n"),
                        QByteArray("\n\n\n\n"),
                    };
                    for (const auto &x : payloads) {
                        auto socket = new QTcpSocket(this);
                        socket->connectToHost(QStringLiteral("localhost"), port);
                        QVERIFY(socket->waitForConnected());
                        socket->write(x);
                    }

                    // Do the actual request a bit later
                    QTimer::singleShot(100ms, this, [this, request] {
                        QCOMPARE(state, CustomState);
                        state = BrowserOpened;
                        this->OAuthTestCase::createBrowserReply(request);
                    });
               });
               return nullptr;
            }

            QNetworkReply *tokenReply(QNetworkAccessManager::Operation op, const QNetworkRequest &req) override
            {
                if (state == CustomState)
                    return new FakeErrorReply{op, req, this, 500};
                return OAuthTestCase::tokenReply(op, req);
            }

            void oauthResult(OAuth::Result result, const QString &token, const QString &refreshToken) override
            {
                if (state != CustomState) {
                    return OAuthTestCase::oauthResult(result, token, refreshToken);
                }
                QCOMPARE(result, OAuth::Error);
            }
        } test;
        test.test();
    }

    void testWellKnown() {
        struct Test : OAuthTestCase {
            Test()
            {
                localHost = QStringLiteral("127.0.0.1");
            }

            QNetworkReply * wellKnownReply(QNetworkAccessManager::Operation op, const QNetworkRequest & req) override {
                OC_ASSERT(op == QNetworkAccessManager::GetOperation);
                QJsonDocument jsondata(QJsonObject{
                    {QStringLiteral("authorization_endpoint"),
                        QJsonValue(QStringLiteral("oauthtest://openidserver") + sOAuthTestServer.path() + QStringLiteral("/index.php/apps/oauth2/authorize"))},
                    {QStringLiteral("token_endpoint"), QStringLiteral("oauthtest://openidserver/token_endpoint")}});
                return new FakePayloadReply(op, req, jsondata.toJson(), fakeAm);
            }

            void openBrowserHook(const QUrl & url) override {
                OC_ASSERT(url.host() == QStringLiteral("openidserver"));
                QUrl url2 = url;
                url2.setHost(sOAuthTestServer.host());
                OAuthTestCase::openBrowserHook(url2);
            }

            QNetworkReply *tokenReply(QNetworkAccessManager::Operation op, const QNetworkRequest & request) override
            {
                OC_ASSERT(browserReply);
                OC_ASSERT(request.url().toString().startsWith(QStringLiteral("oauthtest://openidserver/token_endpoint")));
                auto req = request;
                req.setUrl(QUrl(request.url().toString().replace(QLatin1String("oauthtest://openidserver/token_endpoint"),
                    sOAuthTestServer.toString() + QStringLiteral("/index.php/apps/oauth2/api/v1/token"))));
                return OAuthTestCase::tokenReply(op, req);
            }
        } test;
        test.test();
    }


    void testTimeout()
    {
        struct Test : OAuthTestCase
        {
            QScopedValueRollback<std::chrono::seconds> rollback;

            Test()
                : rollback(AbstractNetworkJob::httpTimeout, 1s)
            {
                localHost = QStringLiteral("127.0.0.1");
            }

            QNetworkReply *statusPhpReply(QNetworkAccessManager::Operation op, const QNetworkRequest &req) override
            {
                OC_ASSERT(op == QNetworkAccessManager::GetOperation);
                return new FakeHangingReply(op, req, fakeAm);
            }

            void oauthResult(OAuth::Result result, const QString &token, const QString &refreshToken) override
            {
                Q_UNUSED(token);
                Q_UNUSED(refreshToken);

                QCOMPARE(state, StartState);
                QCOMPARE(result, OAuth::Error);
                gotAuthOk = true;
                replyToBrowserOk = true;
            }
        } test;
        test.test();
    }

    void testDynamicRegistrationFailFallback()
    {
        // similar to testWellKnown but the server announces dynamic client registration
        // when this fails we fall back to the default client id and secret
        struct Test : OAuthTestCase
        {
            Test()
            {
                localHost = QStringLiteral("127.0.0.1");
            }

            QNetworkReply *wellKnownReply(QNetworkAccessManager::Operation op, const QNetworkRequest &req) override
            {
                OC_ASSERT(op == QNetworkAccessManager::GetOperation);
                const QJsonDocument jsondata(QJsonObject{
                    {QStringLiteral("authorization_endpoint"),
                        QJsonValue(QStringLiteral("oauthtest://openidserver") + sOAuthTestServer.path() + QStringLiteral("/index.php/apps/oauth2/authorize"))},
                    {QStringLiteral("token_endpoint"), QStringLiteral("oauthtest://openidserver/token_endpoint")},
                    {QStringLiteral("registration_endpoint"), QStringLiteral("%1/clients-registrations").arg(localHost)}});
                return new FakePayloadReply(op, req, jsondata.toJson(), fakeAm);
            }

            void openBrowserHook(const QUrl &url) override
            {
                OC_ASSERT(url.host() == QStringLiteral("openidserver"));
                QUrl url2 = url;
                url2.setHost(sOAuthTestServer.host());
                OAuthTestCase::openBrowserHook(url2);
            }

            QNetworkReply *tokenReply(QNetworkAccessManager::Operation op, const QNetworkRequest &request) override
            {
                OC_ASSERT(browserReply);
                OC_ASSERT(request.url().toString().startsWith(QStringLiteral("oauthtest://openidserver/token_endpoint")));
                auto req = request;
                qDebug() << request.url() << request.url().query();
                req.setUrl(QUrl(request.url().toString().replace(QLatin1String("oauthtest://openidserver/token_endpoint"),
                    sOAuthTestServer.toString() + QStringLiteral("/index.php/apps/oauth2/api/v1/token"))));
                return OAuthTestCase::tokenReply(op, req);
            }
        } test;
        test.test();
    }

    void testDynamicRegistration()
    {
        // similar to testWellKnown but the server announces dynamic client registration
        // this means that the client id and secret are provided by the server
        struct Test : OAuthTestCase
        {
            Test()
            {
                localHost = QStringLiteral("127.0.0.1");
                _expectedClientId = QStringLiteral("3e4ea0f3-59ea-434a-92f2-b0d3b54443e9");
            }

            QNetworkReply *wellKnownReply(QNetworkAccessManager::Operation op, const QNetworkRequest &req) override
            {
                OC_ASSERT(op == QNetworkAccessManager::GetOperation);
                const QJsonDocument jsondata(QJsonObject{
                    {QStringLiteral("authorization_endpoint"),
                        QJsonValue(QStringLiteral("oauthtest://openidserver") + sOAuthTestServer.path() + QStringLiteral("/index.php/apps/oauth2/authorize"))},
                    {QStringLiteral("token_endpoint"), QStringLiteral("oauthtest://openidserver/token_endpoint")},
                    {QStringLiteral("registration_endpoint"), QStringLiteral("%1/clients-registrations").arg(localHost)}});
                return new FakePayloadReply(op, req, jsondata.toJson(), fakeAm);
            }

            void openBrowserHook(const QUrl &url) override
            {
                OC_ASSERT(url.host() == QStringLiteral("openidserver"));
                QUrl url2 = url;
                url2.setHost(sOAuthTestServer.host());
                OAuthTestCase::openBrowserHook(url2);
            }

            QNetworkReply *tokenReply(QNetworkAccessManager::Operation op, const QNetworkRequest &request) override
            {
                OC_ASSERT(browserReply);
                OC_ASSERT(request.url().toString().startsWith(QStringLiteral("oauthtest://openidserver/token_endpoint")));
                auto req = request;
                qDebug() << request.url() << request.url().query();
                req.setUrl(QUrl(request.url().toString().replace(QStringLiteral("oauthtest://openidserver/token_endpoint"),
                    sOAuthTestServer.toString() + QStringLiteral("/index.php/apps/oauth2/api/v1/token"))));
                return OAuthTestCase::tokenReply(op, req);
            }

            QNetworkReply *clientRegistrationReply(QNetworkAccessManager::Operation op, const QNetworkRequest &request) override
            {
                const QByteArray payload(QByteArrayLiteral("{\"redirect_uris\":[\"http://127.0.0.1\"],\"token_endpoint_auth_method\":\"client_secret_basic\",\"grant_types\":[\"authorization_code\",\"refresh_token\"],\"response_types\":[\"code\",\"none\"],\"client_id\":\"3e4ea0f3-59ea-434a-92f2-b0d3b54443e9\",\"client_secret\":\"rmoEXFc1Z5tGTApxanBW7STlWODqRTYx\",\"client_name\":\"ownCloud 3.0.0.0\",\"scope\":\"web-origins address phone offline_access microprofile-jwt\",\"subject_type\":\"public\",\"request_uris\":[],\"tls_client_certificate_bound_access_tokens\":false,\"client_id_issued_at\":1663074650,\"client_secret_expires_at\":0,\"registration_client_uri\":\"https://someserver.de/auth/realms/ocis/clients-registrations/openid-connect/3e4ea0f3-59ea-434a-92f2-b0d3b54443e9\",\"registration_access_token\":\"eyJhbGciOiJIUzI1NiIsInR5cCIgOiAiSldUIiwia2lkIiA6ICIzYjQ2YWVkYi00Y2I3LTRiMGItODA5Ny1lNjRmOGQ5ZWY2YjQifQ.eyJleHAiOjAsImlhdCI6MTY2MzA3NDY1MCwianRpIjoiNTlkZWIzNTktNTBmZS00YTUyLWFmNTItZjFjNDg3ZTFlOWRmIiwiaXNzIjoiaHR0cHM6Ly9rZXljbG9hay5vd25jbG91ZC5jbG91ZHNwZWljaGVyLWJheWVybi5kZS9hdXRoL3JlYWxtcy9vY2lzIiwiYXVkIjoiaHR0cHM6Ly9rZXljbG9hay5vd25jbG91ZC5jbG91ZHNwZWljaGVyLWJheWVybi5kZS9hdXRoL3JlYWxtcy9vY2lzIiwidHlwIjoiUmVnaXN0cmF0aW9uQWNjZXNzVG9rZW4iLCJyZWdpc3RyYXRpb25fYXV0aCI6ImFub255bW91cyJ9.v1giSvpnKw1hTtBYZaqdp3JqnZ5mvCKYhQDKkT7x8Us\",\"backchannel_logout_session_required\":false,\"require_pushed_authorization_requests\":false}"));

                return new FakePayloadReply(op, request, payload, fakeAm);
            }

        } test;
        test.test();
    }
};


QTEST_MAIN(TestOAuth)
#include "testoauth.moc"
