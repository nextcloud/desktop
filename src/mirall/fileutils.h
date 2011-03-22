
#ifndef MIRALL_FILEUTILS_H
#define MIRALL_FILEUTILS_H

#include <QStringList>

namespace Mirall
{

class FileUtils
{
public:
    enum SubFolderListOption {
        SubFolderNoOptions = 0x0,
        SubFolderRecursive = 0x1,
    };
    Q_DECLARE_FLAGS(SubFolderListOptions, SubFolderListOption)

    static QStringList subFoldersList(QString folder,
                                      SubFolderListOptions options = SubFolderNoOptions );
    static bool removeDir(const QString &path);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(FileUtils::SubFolderListOptions)

}

#endif
