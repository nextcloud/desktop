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
#include <QFile>
#include <QTextStream>
#include <qmutex.h>

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

  void postGuiLog(const QString& title, const QString& message);
  void postOptionalGuiLog(const QString& title, const QString& message);
  void postGuiMessage(const QString& title, const QString& message);

  void setLogFile( const QString & name );
  void setLogExpire( int expire );
  void setLogDir( const QString& dir );
  void setLogFlush( bool flush );

signals:
  void newLog(const QString&);
  void guiLog(const QString&, const QString&);
  void guiMessage(const QString&, const QString&);
  void optionalGuiLog(const QString&, const QString&);

public slots:
  void enterNextLogFile();

protected:
  Logger(QObject* parent=0);
  QList<Log> _logs;
  bool       _showTime;
  bool       _doLogging;

  static Logger* _instance;

  QFile       _logFile;
  bool        _doFileFlush;
  int         _logExpire;
  QScopedPointer<QTextStream> _logstream;
  QMutex      _mutex;
  QString     _logDirectory;

};

} // namespace Mirall

#endif // LOGGER_H
