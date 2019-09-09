/*
 * Copyright (C) by Camila Ayres <hello@camila.codes>
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

#ifndef ICONJOB_H
#define ICONJOB_H

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
class IconJob : public QObject
{
    Q_OBJECT
public:
    explicit IconJob(const QUrl &url, QObject *parent = nullptr);

signals:
    void jobFinished(QByteArray iconData);

private slots:
    void finished(QNetworkReply *reply);

private:
    QNetworkAccessManager _accessManager;
};
}

#endif // ICONJOB_H
