#pragma once

#include <QUrlQuery>
#include <QDateTime>
#include <QTimer>
#include <QNetworkRequest>
#include <QQueue>
#include "ganalytics.h"
#include "abstractnetworkjob.h"

#include <QJSonObject>
#include <map>
#include "account.h"

struct QueryBuffer
{
    QString eventValue;
    QString screenNameValue;
    QDateTime time;
};

class GAnalyticsWorker : public QObject
{
    Q_OBJECT


public:
    explicit GAnalyticsWorker(GAnalytics *parent = 0);

    GAnalytics *q;

    QNetworkAccessManager *networkManager = nullptr;

    QQueue<QueryBuffer> m_messageQueue;
    QTimer m_timer;
    QNetworkRequest m_request;
    GAnalytics::LogLevel m_logLevel;

    QString m_measurementId;
    QString m_clientID;
    QString m_userID;
    QString m_appName;
    QString m_appVersion;
    QString m_language;
    QString m_screenResolution;
    QString m_viewportSize;

    OCC::AccountPtr m_account;

    bool m_anonymizeIPs = false;
    bool m_isEnabled = false;
    int m_timerInterval = 30000;
    bool m_validation = false;

    const static int fourHours = 4 * 60 * 60 * 1000;
    const static QLatin1String dateTimeFormat;

public:
    void logMessage(GAnalytics::LogLevel level, const QString &message);

    QString getScreenResolution();
    QString getUserAgent();

    void enqueQueryWithCurrentTime(QString eventValue, QString screenNameValue);
    void setIsSending(bool doSend);
    void enable(bool state);

public slots:
    void postMessage();
    void postMessageFinished();

private:
    void setStaticQueryValues(QUrlQuery& query);
    void setDynamicQueryValues(QUrlQuery& query, const QString& eventValue, const QString& screenNameValue);

    enum GA4
    {
        Version,
        MeasurementID,
        ClientID,
        SessionID,
        SessionSequence,
        SessionCount,
        UserID,
        Language,
        ScreenResolution,
        AgentArch,
        AgentMobileBrand,
        AgentPlatform,
        AgentPlatformVersion,
        Event,
        ScreenName,
        AppName,
        AppVersion,
        EngagementTime,
    };

    std::map<GA4, QString> _ga4 = { 
        { Version, "v" }, 
        { MeasurementID, "tid" }, 
        { ClientID, "cid" },
        { SessionID, "sid" },
        { SessionSequence, "_s" },
        { SessionCount, "sct" },
        { UserID, "uid" },
        { Language, "ul" },
        { ScreenResolution, "sr" },
        { AgentArch, "ua" },
        { AgentMobileBrand, "uamb" },
        { AgentPlatform, "uap" },
        { AgentPlatformVersion, "uapv" },
        { Event, "en" },
        { ScreenName, "ep.screen_name" },
        { AppName, "ep.app_name" },
        { AppVersion, "ep.software_version" },
        { EngagementTime, "_et" },
    }; 

    friend class GAnalytics;
};

