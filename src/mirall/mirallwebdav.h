/*
 * Copyright (C) by Klaas Freitag <freitag@kde.org>
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
#ifndef MIRALLWEBDAV_H
#define MIRALLWEBDAV_H

#include <QObject>

#include <qwebdav.h>

class MirallWebDAV : public QObject
{
    Q_OBJECT
public:
  explicit MirallWebDAV(QObject *parent = 0);

  bool httpConnect( const QString& url, const QString&, const QString& );

  bool mkdir( const QString& dir );

protected:

signals:
  void webdavFinished( QNetworkReply* );

public slots:

private:
  QString _host;
  QString _user;
  QString _passwd;
  QString _error;

  QWebdav *_webdav;
};

#endif // MIRALLWEBDAV_H
