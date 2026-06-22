/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2016 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef NOTIFICATIONCONFIRMJOB_H
#define NOTIFICATIONCONFIRMJOB_H

#include "accountfwd.h"
#include "abstractnetworkjob.h"

#include <QVector>
#include <QList>
#include <QPair>
#include <QUrl>

namespace OCC {

/**
 * @brief The NotificationConfirmJob class
 * @ingroup gui
 *
 * Class to call an action-link of a notification coming from the server.
 * All the communication logic is handled in this class.
 *
 */
class NotificationConfirmJob : public AbstractNetworkJob
{
    Q_OBJECT

public:
    explicit NotificationConfirmJob(AccountPtr account);

    /**
     * @brief Set the verb and link for the job
     *
     * @param verb currently supported GET PUT POST DELETE
     */
    void setLinkAndVerb(const QUrl &link, const QByteArray &verb);

    /**
     * @brief Start the OCS request
     */
    void start() override;

signals:

    /**
     * Result of the OCS request
     *
     * @param reply the reply
     */
    void jobFinished(QString reply, int replyCode);

private slots:
    bool finished() override;

private:
    QByteArray _verb;
    QUrl _link;
};
}

#endif // NotificationConfirmJob_H
