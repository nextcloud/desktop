/*
 * Copyright (C) by Krzesimir Nowak <krzesimir@endocode.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include <QDebug>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTextStream>

#include "mirall/mirallaccessmanager.h"
#include "mirall/utility.h"

namespace Mirall
{

namespace
{

QString
operationToString (QNetworkAccessManager::Operation op)
{
  switch (op) {
  case QNetworkAccessManager::HeadOperation:
    return QString::fromLatin1 ("HEAD");

  case QNetworkAccessManager::GetOperation:
    return QString::fromLatin1 ("GET");

  case QNetworkAccessManager::PutOperation:
    return QString::fromLatin1 ("PUT");

  case QNetworkAccessManager::PostOperation:
    return QString::fromLatin1 ("POST");

  case QNetworkAccessManager::DeleteOperation:
    return QString::fromLatin1 ("DELETE");

  case QNetworkAccessManager::CustomOperation:
    return QString::fromLatin1 ("CUSTOM");

  case QNetworkAccessManager::UnknownOperation:
    return QString::fromLatin1 ("UNKNOWN");
  }

  return QString::fromLatin1 ("PLAIN WRONG");
}

} // ns

MirallAccessManager::MirallAccessManager(QObject* parent)
    : QNetworkAccessManager (parent)
{}

QNetworkReply* MirallAccessManager::createRequest(QNetworkAccessManager::Operation op, const QNetworkRequest& request, QIODevice* outgoingData)
{
    static unsigned int staticRequestNo(0);

    unsigned int requestNo(staticRequestNo++);
    QNetworkRequest newRequest(request);

    newRequest.setRawHeader( QByteArray("User-Agent"), Utility::userAgentString());

    QNetworkReply* reply(QNetworkAccessManager::createRequest (op, newRequest, outgoingData));

    logRequest(requestNo, op, newRequest, outgoingData);
    connect (reply, SIGNAL(finished()),
             this, SLOT(logReply()));
    reply->setProperty("mirall-request-no", QVariant(requestNo));
    return reply;
}

void MirallAccessManager::logRequest(unsigned int requestNo, QNetworkAccessManager::Operation op, const QNetworkRequest& request, QIODevice* outgoingData)
{
    QString log;
    QTextStream stream (&log);
    QVariant variant = request.attribute (QNetworkRequest::CustomVerbAttribute);

    stream << "\nREQUEST NO: " << requestNo
           << "\nRequest operation: " << operationToString (op)
           << "\nRequest URL: " << request.url ().toString ();
    if (variant.isValid ()) {
        stream << "Request custom operation: " << variant.toByteArray () << "\n";
    }
    stream << "\nRequest headers:\n";
    Q_FOREACH (const QByteArray& header, request.rawHeaderList ()) {
        stream << "  " << header << ": " << request.rawHeader (header) << "\n";
    }
    if (outgoingData) {
        stream << "Body:\n" << outgoingData->peek(outgoingData->bytesAvailable()) << "\n";
    }
    stream << "----------\n";
    qDebug() << log;
}

void MirallAccessManager::logReply()
{
    QNetworkReply* reply = qobject_cast< QNetworkReply* > (sender());

    if (!reply) {
        return;
    }

    disconnect (reply, SIGNAL(finished()),
                this, SLOT(logReply()));

    unsigned int requestNo(reply->property("mirall-request-no").toUInt());
    QString log;
    QTextStream stream (&log);
    QVariant variant = reply->property ("mirall-user");

    stream << "\nREPLY TO REQUEST NO: " << requestNo << "\n";
    if (variant.isValid()) {
        stream << "Auth user: " << variant.toString() << "\n"
               << "Auth password: " << reply->property("mirall-password").toString() << "\n";
    }
    variant = reply->attribute (QNetworkRequest::HttpStatusCodeAttribute);
    if (variant.isValid ()) {
        stream << "Reply status: " << variant.toInt () << "\n";
    }
    variant = reply->attribute (QNetworkRequest::HttpReasonPhraseAttribute);
    if (variant.isValid ()) {
        stream << "Reply reason: " << variant.toByteArray () << "\n";
    }
    variant = reply->attribute (QNetworkRequest::RedirectionTargetAttribute);
    if (variant.isValid ()) {
        stream << "Reply redirection: " << variant.toUrl ().toString () << "\n";
    }
    stream << "Reply headers:\n";
    Q_FOREACH (const QByteArray& header, reply->rawHeaderList ()) {
        stream << "  " << header << ": " << reply->rawHeader (header) << "\n";
    }
    stream << "Reply data:\n"
           << reply->peek (reply->bytesAvailable ())
           << "\n----------\n";
    qDebug() << log;
}

} // ns Mirall
