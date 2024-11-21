#pragma once

#include <QObject>
#include <QVariantMap>
#include "account.h"

class QNetworkAccessManager;
class GAnalyticsWorker;
class DataCollectionWrapper;
class Account;

// https://developers.google.com/analytics/devguides/collection/protocol/ga4/verify-implementation?client_type=gtag
class GAnalytics : public QObject
{
    Q_OBJECT
    Q_ENUMS(LogLevel)

public:
    ~GAnalytics();

    static GAnalytics& getInstance()
    {
        static GAnalytics instance; 
        return instance;
    }

public:
    enum LogLevel
    {
        Debug,
        Info,
        Error
    };

    void setMeasurementId(const QString& measurementId);
    void setClientID(const QString& clientID);

    void setLogLevel(LogLevel logLevel);
    LogLevel logLevel() const;

    void setViewportSize(const QString &viewportSize);
    QString viewportSize() const;

    void setLanguage(const QString &language);
    QString language() const;

    void setAnonymizeIPs(bool anonymize);
    bool anonymizeIPs();

    void setSendInterval(int milliseconds);
    int sendInterval() const;

    void enable(bool state = true);
    bool isEnabled();

    OCC::AccountPtr account;
    void setAccount(OCC::AccountPtr account);
    
    void enableValidation(bool state);

    /// Get or set the network access manager. If none is set, the class creates its own on the first request
    void setNetworkAccessManager(QNetworkAccessManager *networkAccessManager);
    QNetworkAccessManager *networkAccessManager() const;


private:
    void sendEvent(const QString &page = QString(), const QString &element = QString());
    void sendEventImmediatley(const QString &page, const QString &element);
    explicit GAnalytics(QObject *parent = 0);
    Q_DISABLE_COPY_MOVE(GAnalytics);

private:
    GAnalyticsWorker *d;

    friend class DataCollectionWrapper;
};

