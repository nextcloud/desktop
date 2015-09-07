/*
 * Copyright (C) by Roeland Jago Douma <roeland@famdouma.nl>
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

#ifndef OCSJOB_H
#define OCSJOB_H

#include "accountfwd.h"
#include "abstractnetworkjob.h"

#include <QVector>
#include <QList>
#include <QPair>
#include <QUrl>

namespace OCC {

/**
 * @brief The OcsShareJob class
 * @ingroup gui
 */
class OCSJob : public AbstractNetworkJob {
    Q_OBJECT

protected:

    explicit OCSJob(AccountPtr account, QObject* parent = 0);

    /**
     * Set the verb for the job
     *
     * @param verb currently supported PUT POST DELETE
     */
    void setVerb(const QByteArray& verb);

    /**
     * The url of the OCS endpoint
     *
     * @param url
     */
    void setUrl(const QUrl& url);

    /**
     * Set the get parameters to the url
     *
     * @param getParams list of pairs to add to the url
     */
    void setGetParams(const QList<QPair<QString, QString> >& getParams);

    /**
     * Set the post parameters
     *
     * @param postParams list of pairs to add (urlEncoded) to the body of the
     * request
     */
    void setPostParams(const QList<QPair<QString, QString> >& postParams);

    /**
     * List of expected statuscodes for this request
     * A warning will be printed to the debug log if a different status code is
     * encountered
     *
     * @param code Accepted status code
     */
    void addPassStatusCode(int code);

public:
    /**
     * Parse the response and return the status code and the message of the
     * reply (metadata)
     *
     * @param json The reply from OCS
     * @param message The message that is set in the metadata
     * @return The statuscode of the OCS response
     */
    static int getJsonReturnCode(const QVariantMap &json, QString &message);

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
    void jobFinished(QVariantMap reply);

private slots:
    virtual bool finished() Q_DECL_OVERRIDE;

private:
    QByteArray _verb;
    QUrl _url;
    QList<QPair<QString, QString> > _postParams;
    QVector<int> _passStatusCodes;
};

}

#endif // OCSJOB_H
