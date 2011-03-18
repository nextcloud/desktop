#include <cstdlib>
#include <cerrno>
#include <cstring>

#include <QDebug>

#include "mirall/temporarydir.h"
#include "mirall/fileutils.h"

namespace Mirall
{

static char dir_template[] = "/tmp/mirall-XXXXXX";

TemporaryDir::TemporaryDir()
{
    char *tmp = ::mkdtemp(dir_template);
    _path = QString((const char *) tmp);

    //qDebug() << "tmp:" << _path;
    //qDebug() << strerror(errno);

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
