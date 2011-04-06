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

#include <cstdlib>
#include <cerrno>
#include <cstring>

#include <QDebug>
#include <QDir>

#include "mirall/temporarydir.h"
#include "mirall/fileutils.h"

namespace Mirall
{

static QString dirTemplate = QDir::tempPath() + "/mirall-XXXXXX";

TemporaryDir::TemporaryDir()
{
    char *buff = ::strdup(dirTemplate.toLocal8Bit().data());
    char *tmp = ::mkdtemp(buff);
    _path = QString((const char *) tmp);
    ::free(buff);
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
