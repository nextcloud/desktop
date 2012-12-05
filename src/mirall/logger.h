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

#ifndef LOGGER_H
#define LOGGER_H

#include <QObject>
#include <QList>
#include <QDateTime>

namespace Mirall {

struct Log{
  typedef enum{
    Mirall,
    CSync
  } Source;

  QDateTime timeStamp;
  Source source;
  QString message;
};

class Logger : public QObject
{
  Q_OBJECT
public:
  void log(Log log);

  static void csyncLog( const QString& message );
  static void mirallLog( const QString& message );

  const QList<Log>& logs() const {return _logs;}

  static Logger* instance();
  static void destroy();

signals:
  void newLog(const QString&);

protected:
  Logger(QObject* parent=0);
  QList<Log> _logs;
  bool       _showTime;
  bool       _doLogging;

  static Logger* _instance;
};

} // namespace Mirall

#endif // LOGGER_H
