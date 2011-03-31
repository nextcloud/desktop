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
