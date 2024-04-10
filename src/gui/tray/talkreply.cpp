#include "talkreply.h"

#include "accountstate.h"
#include "networkjobs.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace OCC {

Q_LOGGING_CATEGORY(lcTalkReply, "nextcloud.gui.talkreply", QtInfoMsg)

TalkReply::TalkReply(AccountState *accountState, QObject *parent)
    : QObject(parent)
    , _accountState(accountState)
{
    Q_ASSERT(_accountState && _accountState->account());
}

void TalkReply::sendReplyMessage(const QString &conversationToken, const QString &message, const QString &replyTo)
{
    QPointer<JsonApiJob> apiJob =  new JsonApiJob(_accountState->account(),
        QLatin1String("ocs/v2.php/apps/spreed/api/v1/chat/%1").arg(conversationToken),
        this);

    QObject::connect(apiJob, &JsonApiJob::jsonReceived, this, [&](const QJsonDocument &response, const int statusCode) {
        if(statusCode != 200) {
            qCWarning(lcTalkReply) << "Status code" << statusCode;
        }

        const auto responseObj = response.object().value("ocs").toObject().value("data").toObject();
        emit replyMessageSent(responseObj.value("message").toString());

        deleteLater();
    });

    QUrlQuery params;
    params.addQueryItem(QStringLiteral("message"), message);
    params.addQueryItem(QStringLiteral("replyTo"), QString(replyTo));

    apiJob->addQueryParams(params);
    apiJob->setVerb(JsonApiJob::Verb::Post);
    apiJob->start();
}
}
