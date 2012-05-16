/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
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

#ifndef UPDATEDETECTOR_H
#define UPDATEDETECTOR_H

#include <QObject>

class QNetworkAccessManager;
class QNetworkReply;

namespace Mirall {

class Theme;

class UpdateDetector : public QObject
{
    Q_OBJECT
public:
    explicit UpdateDetector(QObject *parent = 0);
    
    void versionCheck( Theme * );
signals:
    
public slots:

protected slots:
    void slotVersionInfoArrived( QNetworkReply* );

private:
    QString getSystemInfo();

    QNetworkAccessManager *_accessManager;
};

}

#endif // UPDATEDETECTOR_H
