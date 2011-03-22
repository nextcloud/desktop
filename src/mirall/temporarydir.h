
#ifndef MIRALL_TEMPORARYDIR_H
#define MIRALL_TEMPORARYDIR_H

#include <QString>

namespace Mirall
{

class TemporaryDir
{
public:
    TemporaryDir();
    ~TemporaryDir();

    QString path() const;

private:
    QString _path;
};

}

#endif
