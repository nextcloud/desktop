
#ifndef MIRALL_FILEUTILS_H
#define MIRALL_FILEUTILS_H

#include <QString>

namespace Mirall
{

class FileUtils
{
public:
    static bool removeDir(const QString &path);
};

}

#endif
