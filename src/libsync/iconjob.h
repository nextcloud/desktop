/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef ICONJOB_H
#define ICONJOB_H

#include "account.h"
#include "accountfwd.h"
#include "owncloudlib.h"

#include <QObject>
#include <QByteArray>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>

namespace OCC {

/**
 * @brief Job to fetch a icon
 * @ingroup gui
 */
class OWNCLOUDSYNC_EXPORT IconJob : public QObject
{
    Q_OBJECT
public:
    explicit IconJob(AccountPtr account, const QUrl &url, QObject *parent = nullptr);

signals:
    void jobFinished(QByteArray iconData);
    void error(QNetworkReply::NetworkError errorType);

private slots:
    void finished();
};
}

#endif // ICONJOB_H
