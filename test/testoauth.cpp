/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#include <QtTest/QtTest>
#include <QDesktopServices>

#include "libsync/creds/oauth.h"
#include "syncenginetestutils.h"
#include "theme.h"
#include "common/asserts.h"

using namespace OCC;

class DesktopServiceHook : public QObject
{
    Q_OBJECT
signals:
    void hooked(const QUrl &);
public:
    DesktopServiceHook() { QDesktopServices::setUrlHandler("oauthtest", this, "hooked"); }
};

static const QUrl sOAuthTestServer("oauthtest://someserver/owncloud");


class FakePostReply : public QNetworkReply
{
    Q_OBJECT
public:
    std::unique_ptr<QIODevice> payload;
    bool aborted = false;
    bool redirectToPolicy = false;
    bool redirectToToken = false;

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
            setError(OperationCanceledError, "Operation Canceled");
            emit metaDataChanged();
            emit finished();
            return;
        } else if (redirectToPolicy) {
            setHeader(QNetworkRequest::LocationHeader, "/my.policy");
            setAttribute(QNetworkRequest::RedirectionTargetAttribute, "/my.policy");
            setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 302); // 302 might or might not lose POST data in rfc
            setHeader(QNetworkRequest::ContentLengthHeader, 0);
            emit metaDataChanged();
            emit finished();
            return;
        } else if (redirectToToken) {
            // Redirect to self
            QVariant destination = QVariant(sOAuthTestServer.toString()+QLatin1String("/index.php/apps/oauth2/api/v1/token"));
            setHeader(QNetworkRequest::LocationHeader, destination);
            setAttribute(QNetworkRequest::RedirectionTargetAttribute, destination);
            setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 307); // 307 explicitly in rfc says to not lose POST data
            setHeader(QNetworkRequest::ContentLengthHeader, 0);
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
        QTimer::singleShot(100, this, [this] { this->FakePostReply::respond(); });
    }
};


class OAuthTestCase : public QObject
{
    Q_OBJECT
    DesktopServiceHook desktopServiceHook;
public:
    enum State { StartState, BrowserOpened, TokenAsked, CustomState } state = StartState;
    Q_ENUM(State);
    bool replyToBrowserOk = false;
    bool gotAuthOk = false;
    virtual bool done() const { return replyToBrowserOk && gotAuthOk; }

    FakeQNAM *fakeQnam = nullptr;
    QNetworkAccessManager realQNAM;
    QPointer<QNetworkReply> browserReply = nullptr;
    QString code = generateEtag();
    OCC::AccountPtr account;

    QScopedPointer<OAuth> oauth;

    virtual void test() {
        fakeQnam = new FakeQNAM({});
        account = OCC::Account::create();
        account->setUrl(sOAuthTestServer);
        account->setCredentials(new FakeCredentials{fakeQnam});
        fakeQnam->setParent(this);
        fakeQnam->setOverride([this](QNetworkAccessManager::Operation op, const QNetworkRequest &req, QIODevice *device)  {
            if (req.url().path().endsWith(".well-known/openid-configuration"))
                return this->wellKnownReply(op, req);
            OC_ASSERT(device);
            OC_ASSERT(device->bytesAvailable() > 0); // OAuth2 always sends around POST data.
            return this->tokenReply(op, req);
        });

        QObject::connect(&desktopServiceHook, &DesktopServiceHook::hooked,
                         this, &OAuthTestCase::openBrowserHook);

        oauth.reset(new OAuth(account.data(), nullptr));
        QObject::connect(oauth.data(), &OAuth::result, this, &OAuthTestCase::oauthResult);
        oauth->startAuthentication();
        QTRY_VERIFY(done());
    }

    virtual void openBrowserHook(const QUrl &url) {
        QCOMPARE(state, StartState);
        state = BrowserOpened;
        QCOMPARE(url.path(), QString(sOAuthTestServer.path() + "/index.php/apps/oauth2/authorize"));
        QVERIFY(url.toString().startsWith(sOAuthTestServer.toString()));
        QUrlQuery query(url);
        QCOMPARE(query.queryItemValue(QLatin1String("response_type")), QLatin1String("code"));
        QCOMPARE(query.queryItemValue(QLatin1String("client_id")), Theme::instance()->oauthClientId());
        QUrl redirectUri(query.queryItemValue(QLatin1String("redirect_uri")));
        QCOMPARE(redirectUri.host(), QLatin1String("localhost"));
        redirectUri.setQuery(QStringLiteral("code=%1&state=%2").arg(code, query.queryItemValue(QStringLiteral("state"))));
        createBrowserReply(QNetworkRequest(redirectUri));
    }

    virtual QNetworkReply *createBrowserReply(const QNetworkRequest &request) {
        browserReply = realQNAM.get(request);
        QObject::connect(browserReply, &QNetworkReply::finished, this, &OAuthTestCase::browserReplyFinished);
        return browserReply;
    }

    virtual void browserReplyFinished() {
        QCOMPARE(sender(), browserReply.data());
        QCOMPARE(state, TokenAsked);
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
        OC_ASSERT(req.url().path() == sOAuthTestServer.path() + "/index.php/apps/oauth2/api/v1/token");
        std::unique_ptr<QBuffer> payload(new QBuffer());
        payload->setData(tokenReplyPayload());
        return new FakePostReply(op, req, std::move(payload), fakeQnam);
    }

    virtual QNetworkReply *wellKnownReply(QNetworkAccessManager::Operation op, const QNetworkRequest &req)
    {
        return new FakeErrorReply(op, req, fakeQnam, 404);
    }

    virtual QByteArray tokenReplyPayload() const {
        // the dummy server provides the user admin
        QJsonDocument jsondata(QJsonObject{
                { "access_token", "123" },
                { "refresh_token" , "456" },
                { "message_url",  "owncloud://success"},
                { "user_id", "admin" },
                { "token_type", "Bearer" }
        });
        return jsondata.toJson();
    }

    virtual void oauthResult(OAuth::Result result, const QString &user, const QString &token , const QString &refreshToken) {
        QCOMPARE(state, TokenAsked);
        QCOMPARE(result, OAuth::LoggedIn);
        QCOMPARE(user, QString("admin"));
        QCOMPARE(token, QString("123"));
        QCOMPARE(refreshToken, QString("456"));
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
                QJsonDocument jsondata(QJsonObject{
                    { "access_token", "123" },
                    { "refresh_token" , "456" },
                    { "message_url",  "owncloud://success"},
                    { "user_id", "wrong_user" },
                    { "token_type", "Bearer" }
                });
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

                std::unique_ptr<QBuffer> payload(new QBuffer);
                payload->setData(tokenReplyPayload());
                return new SlowFakePostReply(op, req, std::move(payload), fakeQnam);
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
                    QVector<QByteArray> payloads = {
                        "GET FOFOFO HTTP 1/1\n\n",
                        "GET /?code=invalie HTTP 1/1\n\n",
                        "GET /?code=xxxxx&bar=fff",
                        QByteArray("\0\0\0", 3),
                        QByteArray("GET \0\0\0 \n\n\n\n\n\0", 14),
                        QByteArray("GET /?code=éléphant\xa5 HTTP\n"),
                        QByteArray("\n\n\n\n"),
                    };
                    foreach (const auto &x, payloads) {
                        auto socket = new QTcpSocket(this);
                        socket->connectToHost("localhost", port);
                        QVERIFY(socket->waitForConnected());
                        socket->write(x);
                    }

                    // Do the actual request a bit later
                    QTimer::singleShot(100, this, [this, request] {
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

            void oauthResult(OAuth::Result result, const QString &user, const QString &token ,
                             const QString &refreshToken) override {
                if (state != CustomState)
                    return OAuthTestCase::oauthResult(result, user, token, refreshToken);
                QCOMPARE(result, OAuth::Error);
            }
        } test;
        test.test();
    }

    void testTokenUrlHasRedirect()
    {
        struct Test : OAuthTestCase {
            int redirectsDone = 0;
            QNetworkReply *tokenReply(QNetworkAccessManager::Operation op, const QNetworkRequest & request) override
            {
                OC_ASSERT(browserReply);
                // Kind of reproduces what we had in https://github.com/owncloud/enterprise/issues/2951 (not 1:1)
                if (redirectsDone == 0) {
                    std::unique_ptr<QBuffer> payload(new QBuffer());
                    payload->setData("");
                    SlowFakePostReply *reply = new SlowFakePostReply(op, request, std::move(payload), this);
                    reply->redirectToPolicy = true;
                    redirectsDone++;
                    return reply;
                } else if  (redirectsDone == 1) {
                    std::unique_ptr<QBuffer> payload(new QBuffer());
                    payload->setData("");
                    SlowFakePostReply *reply = new SlowFakePostReply(op, request, std::move(payload), this);
                    reply->redirectToToken = true;
                    redirectsDone++;
                    return reply;
                } else {
                    // ^^ This is with a custom reply and not actually HTTP, so we're testing the HTTP redirect code
                    // we have in AbstractNetworkJob::slotFinished()
                    redirectsDone++;
                    return OAuthTestCase::tokenReply(op, request);
                }
            }
        } test;
        test.test();
    }

    void testWellKnown() {
        struct Test : OAuthTestCase {
            int redirectsDone = 0;
            QNetworkReply * wellKnownReply(QNetworkAccessManager::Operation op, const QNetworkRequest & req) override {
                OC_ASSERT(op == QNetworkAccessManager::GetOperation);
                QJsonDocument jsondata(QJsonObject{
                    { "authorization_endpoint", QJsonValue(
                            "oauthtest://openidserver" + sOAuthTestServer.path() + "/index.php/apps/oauth2/authorize") },
                    { "token_endpoint" , "oauthtest://openidserver/token_endpoint" }
                });
                return new FakePayloadReply(op, req, jsondata.toJson(), fakeQnam);
            }

            void openBrowserHook(const QUrl & url) override {
                OC_ASSERT(url.host() == "openidserver");
                QUrl url2 = url;
                url2.setHost(sOAuthTestServer.host());
                OAuthTestCase::openBrowserHook(url2);
            }

            QNetworkReply *tokenReply(QNetworkAccessManager::Operation op, const QNetworkRequest & request) override
            {
                OC_ASSERT(browserReply);
                OC_ASSERT(request.url().toString().startsWith("oauthtest://openidserver/token_endpoint"));
                auto req = request;
                req.setUrl(request.url().toString().replace("oauthtest://openidserver/token_endpoint",
                        sOAuthTestServer.toString() + "/index.php/apps/oauth2/api/v1/token"));
                return OAuthTestCase::tokenReply(op, req);
            }
        } test;
        test.test();
    }
};


QTEST_GUILESS_MAIN(TestOAuth)
#include "testoauth.moc"
