#include "tray/talkreply.h"

#include "account.h"
#include "accountstate.h"
#include "syncenginetestutils.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>
#include <QSignalSpy>

namespace {

//reply to message
//https://nextcloud-talk.readthedocs.io/en/latest/chat/#sending-a-new-chat-message
static QByteArray replyToMessageSent = R"({"ocs":{"meta":{"status":"ok","statuscode":201,"message":"OK"},"data":{"id":12,"token":"abc123","actorType":"users","actorId":"user1","actorDisplayName":"User 1","timestamp":1636474603,"message":"test message 2","messageParameters":[],"systemMessage":"","messageType":"comment","isReplyable":true,"referenceId":"","parent":{"id":10,"token":"abc123","actorType":"users","actorId":"user2","actorDisplayName":"User 2","timestamp":1624987427,"message":"test message 1","messageParameters":[],"systemMessage":"","messageType":"comment","isReplyable":true,"referenceId":"2857b6eb77b4d7f1f46c6783513e8ef4a0c7ac53"}}}}
)";

// only send message to chat
static QByteArray replyMessageSent = R"({"ocs":{"meta":{"status":"ok","statuscode":201,"message":"OK"},"data":{"id":11,"token":"abc123","actorType":"users","actorId":"user1","actorDisplayName":"User 1","timestamp":1636474440,"message":"test message 3","messageParameters":[],"systemMessage":"","messageType":"comment","isReplyable":true,"referenceId":""}}}
)";

}

class TestTalkReply : public QObject
{
    Q_OBJECT
        
public:
    TestTalkReply() = default;
    
    OCC::AccountPtr account;
    QScopedPointer<FakeQNAM> fakeQnam;
    QScopedPointer<OCC::AccountState> accountState;

private slots:
    void initTestCase()
    {
        fakeQnam.reset(new FakeQNAM({}));
        account = OCC::Account::create();
        account->setCredentials(new FakeCredentials{fakeQnam.data()});
        account->setUrl(QUrl(("http://example.de")));
        accountState.reset(new OCC::AccountState(account));
        
        fakeQnam->setOverride([this](QNetworkAccessManager::Operation op, const QNetworkRequest &req, QIODevice *device) {
            Q_UNUSED(device);
            QNetworkReply *reply = nullptr;
            
            const auto urlQuery = QUrlQuery(req.url());
            const auto message = urlQuery.queryItemValue(QStringLiteral("message"));
            const auto replyTo = urlQuery.queryItemValue(QStringLiteral("replyTo"));
            const auto path = req.url().path();

            if (path.startsWith(QStringLiteral("/ocs/v2.php/apps/spreed/api/v1/chat")) && replyTo.isEmpty()) {
                reply = new FakePayloadReply(op, req, replyMessageSent, fakeQnam.data());
            } else if (path.startsWith(QStringLiteral("/ocs/v2.php/apps/spreed/api/v1/chat")) && !replyTo.isEmpty()) {
                reply = new FakePayloadReply(op, req, replyToMessageSent, fakeQnam.data());
            }
            
            if (!reply) {
                return qobject_cast<QNetworkReply*>(new FakeErrorReply(op, req, this, 404, QByteArrayLiteral("{error: \"Not found!\"}")));
            }
            
            return reply;
        });

    }

    void testSendReplyMessage_noReplyToSet_messageIsSent()
    {
        QPointer<OCC::TalkReply> talkReply = new OCC::TalkReply(accountState.data());
        const auto message = QStringLiteral("test message 3");
        talkReply->sendReplyMessage(QStringLiteral("abc123"), message);
        QSignalSpy replyMessageSent(talkReply.data(), &OCC::TalkReply::replyMessageSent);
        QVERIFY(replyMessageSent.wait());
        QList<QVariant> arguments = replyMessageSent.takeFirst();
        QVERIFY(arguments.at(0).toString() == message);
    }

    void testSendReplyMessage_replyToSet_messageIsSent()
    {
        QPointer<OCC::TalkReply> talkReply = new OCC::TalkReply(accountState.data());
        const auto message = QStringLiteral("test message 2");
        talkReply->sendReplyMessage(QStringLiteral("abc123"), message, QStringLiteral("11"));
        QSignalSpy replyMessageSent(talkReply.data(), &OCC::TalkReply::replyMessageSent);
        QVERIFY(replyMessageSent.wait());
        QList<QVariant> arguments = replyMessageSent.takeFirst();
        QVERIFY(arguments.at(0).toString() == message);
    }
};

QTEST_MAIN(TestTalkReply)
#include "testtalkreply.moc"
