/*
 * Copyright (C) Hannah von Reth <hannah.vonreth@owncloud.com>
 * Copyright (C) Fabian Müller <fmueller@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#pragma once

#include <QNetworkAccessManager>
#include <QNetworkReply>

#include "abstractnetworkjob.h"
#include "owncloudlib.h"

namespace OCC {

/**
 * This class manages an (HTTP) network job's result. It holds a result on success and error details on failures.
 * The class is universally usable for all kinds of network requests, there is no difference in handling the responses.
 * Instead, instances are created that start a suitable request and wire up the signals accordingly.
 * In contrast to the traditional network jobs (e.g., SimpleNetworkJob), core jobs are not bound to an account. Therefore,
 * they can be used with ease in situations where an account object is not available (e.g., the new wizard).
 */
class OWNCLOUDSYNC_EXPORT CoreJob : public QObject
{
    Q_OBJECT

    friend class AbstractCoreJobFactory;

public:
    explicit CoreJob(QNetworkReply *reply, QObject *parent);

    [[nodiscard]] const QVariant &result() const;

    [[nodiscard]] const QString &errorMessage() const;

    [[nodiscard]] QNetworkReply *reply() const;

    [[nodiscard]] bool success() const;

protected:
    /**
     * Set job result. This emits the finished() signal, and marks the job as successful.
     * The job result or error can be set only once.
     * @param result job result
     */
    void setResult(const QVariant &result);

    /**
     * Set job error details. This emits the finished() signal, and marks the job as failed.
     * The job result or error can be set only once.
     * @param errorMessage network error or other suitable error message
     */
    void setError(const QString &errorMessage);

Q_SIGNALS:
    /**
     * Emitted when a result or error is set. May be emitted once only.
     */
    void finished();

    /**
     * Emitted whenever the user accepts a CA certificate which is not trusted yet using the TlsErrorDialog.
     * Handlers should store the certificate in the access manager passed to the corresponding job.
     * @param caCertificate accepted CA certificate
     */
    void caCertificateAccepted(const QSslCertificate &caCertificate);

private:
    // job result/error should be set only once, because that emits the "finished" signal
    void assertNotFinished();

    bool _success = false;

    QVariant _result;

    QString _errorMessage;
    QNetworkReply *_reply = nullptr;
};


/**
 * Abstract base class for core job factories.
 *
 * Jobs are built by the startJob factory method, which creates a job instance as well as a network request, wires the required signals up, then sends the request.
 */
class OWNCLOUDSYNC_EXPORT AbstractCoreJobFactory
{
public:
    /**
     * Create a new instance
     * @param nam network access manager used to send the requests
     * @param parent optional parent which will be set at this object's parent as well as all jobs' parent.
     */
    explicit AbstractCoreJobFactory(QNetworkAccessManager *nam);
    virtual ~AbstractCoreJobFactory();

    /**
     * Send network request and return associated job.
     * @param url URL to send request to
     * @return job
     */
    virtual CoreJob *startJob(const QUrl &url, QObject *parent) = 0;

protected:
    [[nodiscard]] QNetworkAccessManager *nam() const;

    /**
     * Set job result. Needed because the jobs' methods are protected, and this class is a friend of Job.
     * The job result or error can be set only once.
     * @param result job result
     */
    static void setJobResult(CoreJob *job, const QVariant &result);

    /**
     * Set job error details. Needed because the jobs' methods are protected, and this class is a friend of Job.
     * @param errorMessage network error or other suitable error message
     */
    static void setJobError(CoreJob *job, const QString &errorMessage);

    /**
     * Factory to create QNetworkRequests with properly set timeout.
     */
    template <typename... Params>
    static QNetworkRequest makeRequest(Params... params)
    {
        auto request = QNetworkRequest(params...);

        const auto timeoutMilliseconds = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(AbstractNetworkJob::httpTimeout).count());
        request.setTransferTimeout(timeoutMilliseconds);

        return request;
    }

private:
    QNetworkAccessManager *_nam;
};
}
