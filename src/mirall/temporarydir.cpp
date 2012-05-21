/*
 * Copyright (C) by Duncan Mac-Vicar P. <duncan@kde.org>
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

#include "mirall/temporarydir.h"
#include "mirall/fileutils.h"

#include <cstdlib>
#include <cerrno>
#include <cstring>

#include <QDebug>
#include <QDir>
#include <QDateTime>

namespace Mirall
{

static const QString dirTemplate = QDir::tempPath() + "/mirall-XXXXXX";

/*
 * This class creates a temporary directory as Qt 4.x does not provide an
 * implementation yet. It will come with Qt5, don't forget to PORT
 *
 * This class is only used in the test suite of mirall.
 * As the mkdtemp function is problematic on non linux platforms, I change
 * the filename generation to something more predictable using the timestamp.
 *
 * Note: This is not allowed for cases where the dir name needs to be
 * unpredictable for security reasons.
 */

TemporaryDir::TemporaryDir()
{
#ifdef Q_WS_WIN
  QDateTime dt = QDateTime::currentDateTime();
  QString p( QDir::tempPath() + "/mirall-" + dt.toString( Qt::ISODate ) );
  QDir dir( p );
  if( dir.mkpath( p ) ) {
    _path = p;
  }
#else
    char *buff = ::strdup(dirTemplate.toLocal8Bit().data());
    char *tmp = ::mkdtemp(buff);
    _path = QString((const char *) tmp);
    ::free(buff);
#endif
}

TemporaryDir::~TemporaryDir()
{
    FileUtils::removeDir(_path);
}

QString TemporaryDir::path() const
{
    return _path;
}

}
