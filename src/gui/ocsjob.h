/*
 * Copyright (C) by Roeland Jago Douma <roeland@famdouma.nl>
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

#ifndef OCSJOB_H
#define OCSJOB_H

#include "accountfwd.h"
#include "abstractnetworkjob.h"

#include <QVector>
#include <QList>
#include <QPair>
#include <QUrl>

#define OCS_SUCCESS_STATUS_CODE 100

class QJsonDocument;

namespace OCC {

/**
 * @brief The OcsShareJob class
 * @ingroup gui
 *
 * Base class for jobs that talk to the OCS endpoints on the server.
 * All the communication logic is handled in this class.
 *
 * All OCS jobs (e.g. sharing) should extend this class.
 */
class OcsJob : public AbstractNetworkJob
{
    Q_OBJECT

protected:
    explicit OcsJob(AccountPtr account);

    /**
     * Set the verb for the job
     *
     * @param verb currently supported PUT POST DELETE
     */
    void setVerb(const QByteArray &verb);

    /**
     * Add a new parameter to the request.
     * Depending on the verb this is GET or POST parameter
     *
     * @param name The name of the parameter
     * @param value The value of the parameter
     */
    void addParam(const QString &name, const QString &value);

    /**
     * Set the post parameters
     *
     * @param postParams list of pairs to add (urlEncoded) to the body of the
     * request
     */
    void setPostParams(const QList<QPair<QString, QString>> &postParams);

    /**
     * List of expected statuscodes for this request
     * A warning will be printed to the debug log if a different status code is
     * encountered
     *
     * @param code Accepted status code
     */
    void addPassStatusCode(int code);

    /**
     * The base path for an OcsJob is always the same. But it could be the case that
     * certain operations need to append something to the URL.
     *
     * This function appends the common id. so <PATH>/<ID>
     */
    void appendPath(const QString &id);

public:
    /**
     * Parse the response and return the status code and the message of the
     * reply (metadata)
     *
     * @param json The reply from OCS
     * @param message The message that is set in the metadata
     * @return The statuscode of the OCS response
     */
    static int getJsonReturnCode(const QJsonDocument &json, QString &message);

protected slots:

    /**
     * Start the OCS request
     */
    void start() Q_DECL_OVERRIDE;

signals:

    /**
     * Result of the OCS request
     *
     * @param reply the reply
     */
    void jobFinished(QJsonDocument reply);

    /**
     * The status code was not one of the expected (passing)
     * status code for this command
     *
     * @param statusCode The actual status code
     * @param message The message provided by the server
     */
    void ocsError(int statusCode, const QString &message);

private slots:
    virtual bool finished() Q_DECL_OVERRIDE;

private:
    QByteArray _verb;
    QList<QPair<QString, QString>> _params;
    QVector<int> _passStatusCodes;
};
}

#endif // OCSJOB_H
