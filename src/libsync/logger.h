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

#include "utility.h"

namespace OCC {

struct Log{
  typedef enum{
    Occ,
    CSync
  } Source;

  QDateTime timeStamp;
  Source source;
  QString message;
};

/**
 * @brief The Logger class
 * @ingroup libsync
 */
class OWNCLOUDSYNC_EXPORT Logger : public QObject
{
  Q_OBJECT
public:

  bool isNoop() const;
  void log(Log log);
  void doLog(const QString &log);

  static void mirallLog( const QString& message );

  const QList<Log>& logs() const {return _logs;}

  static Logger* instance();

  void postGuiLog(const QString& title, const QString& message);
  void postOptionalGuiLog(const QString& title, const QString& message);
  void postGuiMessage(const QString& title, const QString& message);

  void setLogWindowActivated(bool activated);
  void setLogFile( const QString & name );
  void setLogExpire( int expire );
  void setLogDir( const QString& dir );
  void setLogFlush( bool flush );

signals:
  void logWindowLog(const QString&);

  void guiLog(const QString&, const QString&);
  void guiMessage(const QString&, const QString&);
  void optionalGuiLog(const QString&, const QString&);

public slots:
  void enterNextLogFile();

private:
  Logger(QObject* parent=0);
  ~Logger();
  QList<Log> _logs;
  bool       _showTime;
  bool       _logWindowActivated;
  QFile       _logFile;
  bool        _doFileFlush;
  int         _logExpire;
  QScopedPointer<QTextStream> _logstream;
  QMutex      _mutex;
  QString     _logDirectory;

};

} // namespace OCC

#endif // LOGGER_H
