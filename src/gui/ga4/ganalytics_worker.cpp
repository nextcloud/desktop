/*
 * This file is part of Nextcloud Destop - Ionos HiDrive Next
 *
 * Modifications:
 * - Changed the usage of Measurement Protocol to the GA4 API
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 * http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "ganalytics.h"
#include "ganalytics_worker.h"
#include <QCoreApplication>
#include <QNetworkAccessManager>
#include <QNetworkReply>

#include <QGuiApplication>
#include <QScreen>

#include <QJsonDocument>
#include <QJsonArray>
#include <QJSonObject>
#include <QNetworkProxy>

#include "logger.h"
#include "account.h"

const QLatin1String GAnalyticsWorker::dateTimeFormat("yyyy,MM,dd-hh:mm::ss:zzz");

Q_LOGGING_CATEGORY(lcGAnalyticsWorker, "nextcloud.gui.ga4.ganalytics_worker", QtInfoMsg)

GAnalyticsWorker::GAnalyticsWorker(GAnalytics *parent)
    : QObject(parent), q(parent), m_logLevel(GAnalytics::Error)
{
    m_appName = QCoreApplication::instance()->applicationName();
    m_appVersion = QCoreApplication::instance()->applicationVersion();
    m_request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json; charset=utf-8");
    m_request.setHeader(QNetworkRequest::UserAgentHeader, getUserAgent());

    m_language = QLocale::system().name().toLower().replace("_", "-");
    m_screenResolution = getScreenResolution();

    m_timer.setInterval(m_timerInterval);
    connect(&m_timer, &QTimer::timeout, this, &GAnalyticsWorker::postMessage);
}

void GAnalyticsWorker::enable(bool state)
{
    // state change to the same is not valid.
    if(m_isEnabled == state)
    {
        return;
    }

    m_isEnabled = state;
    if(m_isEnabled)
    {
        // enable -> start doing things :)
        m_timer.start();
    }
    else
    {
        // disable -> stop the timer
        m_timer.stop();
    }
}

void GAnalyticsWorker::logMessage(GAnalytics::LogLevel level, const QString &message)
{
    if (m_logLevel > level)
    {
        return;
    }
    if(level == GAnalytics::Error)
    {
        // log error message
        qCCritical(lcGAnalyticsWorker) << "[Analytics]" << message;
    }
    else if(level == GAnalytics::Info)
    {
        // log info message
        qCInfo(lcGAnalyticsWorker) << "[Analytics]" << message;
    }
    else if(level == GAnalytics::Debug)
    {
        // log debug message
        qCDebug(lcGAnalyticsWorker) << "[Analytics]" << message;
    }
}

/**
 * Get primary screen resolution.
 * @return      A QString like "800x600".
 */
QString GAnalyticsWorker::getScreenResolution()
{
    QScreen *screen = QGuiApplication::primaryScreen();
    QSize size = screen->size();

    return QString("%1x%2").arg(size.width()).arg(size.height());
}

/**
 * Try to gain information about the system where this application
 * is running. It needs to get the name and version of the operating
 * system, the language and screen resolution.
 * All this information will be send in POST messages.
 * @return agent        A QString with all the information formatted for a POST message.
 */
QString GAnalyticsWorker::getUserAgent()
{
    return QString("%1/%2").arg(m_appName).arg(m_appVersion);
}


/**
 * Takes a QUrlQuery object and wrapp it together with
 * a QTime object into a QueryBuffer struct. These struct
 * will be stored in the message queue.
 */
void GAnalyticsWorker::enqueQueryWithCurrentTime(QString eventValue, QString screenNameValue)
{
    if(!m_isEnabled)
    {
        return;
    }
    QueryBuffer buffer;
    buffer.eventValue = eventValue;
    buffer.screenNameValue = screenNameValue;
    buffer.time = QDateTime::currentDateTime();

    m_messageQueue.enqueue(buffer);
}

/**
 * This function is called by a timer interval.
 * The function tries to send a messages from the queue.
 * If message was successfully send then this function
 * will be called back to send next message.
 * If message queue contains more than one message then
 * the connection will kept open.
 * The message POST is asyncroniously when the server
 * answered a signal will be emitted.
 */
void GAnalyticsWorker::postMessage()
{
    if (m_messageQueue.isEmpty())
    {
        // queue empty -> try sending later
        m_timer.start();
        return;
    }
    else
    {
        // queue has messages -> stop timer and start sending
        m_timer.stop();
    }

    if(m_account == nullptr)
    {
        logMessage(GAnalytics::Error, "account is not set!");
        return;
    }

    QString connection = "close";
    if (m_messageQueue.count() > 1)
    {
        connection = "keep-alive";
    }

    QueryBuffer buffer = m_messageQueue.head();
    QDateTime sendTime = QDateTime::currentDateTime();
    qint64 timeDiff = buffer.time.msecsTo(sendTime);

    if (timeDiff > fourHours)
    {
        // too old.
        m_messageQueue.dequeue();
        emit postMessage();
        return;
    }

    m_request.setRawHeader("Connection", connection.toUtf8());
    m_request.setHeader(QNetworkRequest::ContentLengthHeader, 0);

	if (m_measurementId.isEmpty()) {
		logMessage(GAnalytics::Error, "google analytics measurement id was not set!");
        m_messageQueue.dequeue();
        return;
	}
    if (m_clientID.isEmpty()) {
		logMessage(GAnalytics::Error, "client id was not set!");
        m_messageQueue.dequeue();
        return;
	}

    QUrl requestUrl;
    requestUrl.setScheme("https");
    requestUrl.setHost("www.google-analytics.com");
    if(m_validation){
        requestUrl.setPath("/debug/g/collect");
    }
    else {
        requestUrl.setPath("/g/collect");
    }

    QUrlQuery query;
    setStaticQueryValues(query);
    setDynamicQueryValues(query, buffer.eventValue, buffer.screenNameValue);

    requestUrl.setQuery(query);

	m_request.setUrl(QUrl(requestUrl));

    char message[512];
    snprintf(message, sizeof(message), "%s\n", requestUrl.toString().toStdString().c_str());
    logMessage(GAnalytics::Debug, message);

    QNetworkReply *reply = m_account->sendRawRequest("POST", m_request.url(), m_request, QByteArray());

    connect(reply, SIGNAL(finished()), this, SLOT(postMessageFinished()));
}

void GAnalyticsWorker::setDynamicQueryValues(QUrlQuery& query, const QString& eventValue, const QString& screenNameValue){
    query.addQueryItem(_ga4[GA4::Event], eventValue);
    query.addQueryItem(_ga4[GA4::ScreenName], screenNameValue);
}

void GAnalyticsWorker::setStaticQueryValues(QUrlQuery& query){
    
    query.addQueryItem(_ga4[GA4::Version], "2");    
    query.addQueryItem(_ga4[GA4::MeasurementID], m_measurementId);      
    query.addQueryItem(_ga4[GA4::ClientID], m_clientID);

    query.addQueryItem(_ga4[GA4::SessionID], "1");
    query.addQueryItem(_ga4[GA4::SessionSequence], "1");
    query.addQueryItem(_ga4[GA4::SessionCount], "1");
    query.addQueryItem(_ga4[GA4::UserID], m_userID);
    query.addQueryItem(_ga4[GA4::Language], m_language);
    query.addQueryItem(_ga4[GA4::ScreenResolution], m_screenResolution);
    // TODO SES-169
    // query.addQueryItem(_ga4[GA4::AgentArch], "x86_64");
    query.addQueryItem(_ga4[GA4::AgentMobileBrand], "0");
    #ifdef Q_OS_WIN
        query.addQueryItem(_ga4[GA4::AgentPlatform], "Windows");
    #endif
    #ifdef Q_OS_LINUX
        query.addQueryItem(_ga4[GA4::AgentPlatform], "Linux");
    #endif
    #ifdef Q_OS_MAC
        query.addQueryItem(_ga4[GA4::AgentPlatform], "MacOS");
    #endif
    // TODO SES-169
    // query.addQueryItem(_ga4[GA4::AgentPlatformVersion], "10");
    query.addQueryItem(_ga4[GA4::EngagementTime], "100");

    query.addQueryItem(_ga4[GA4::AppName], QUrl::toPercentEncoding(m_appName));
    query.addQueryItem(_ga4[GA4::AppVersion], m_appVersion);
}


/**
 * NetworkAccsessManager has finished to POST a message.
 * If POST message was successfully send then the message
 * query should be removed from queue.
 * SIGNAL "postMessage" will be emitted to send next message
 * if there is any.
 * If message couldn't be send then next try is when the
 * timer emits its signal.
 */
void GAnalyticsWorker::postMessageFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    
    int httpStausCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (httpStausCode < 200 || httpStausCode > 299)
    {
        logMessage(GAnalytics::Error, QString("Error posting message: %1").arg(reply->errorString()));

        // An error ocurred. Try sending later.
        m_timer.start();
        return;
    }
    else
    {
        logMessage(GAnalytics::Debug, "Message sent");
    }

    if(m_validation)
    {
        QByteArray bytes = reply->readAll();
        QString str = QString::fromUtf8(bytes.data(), bytes.size());
        char message[256];
        snprintf(message, sizeof(message), "Status Code:%d\nReply: %s\n", httpStausCode, str.toStdString().c_str());
        logMessage(GAnalytics::Debug, message);
    }

    m_messageQueue.dequeue();
    postMessage();
    reply->deleteLater();
}