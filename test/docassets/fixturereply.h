/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once
#include <QNetworkReply>

namespace OCC::DocAssets
{
class FixtureReply : public QNetworkReply
{
public:
    FixtureReply(QNetworkAccessManager::Operation operation,
                 const QNetworkRequest &request,
                 const QByteArray &payload,
                 const QByteArray &contentType,
                 QObject *parent);
    void abort() override;
    qint64 bytesAvailable() const override;

protected:
    qint64 readData(char *data, qint64 maximum) override;

private:
    QByteArray _payload;
    qsizetype _offset = 0;
};
}
