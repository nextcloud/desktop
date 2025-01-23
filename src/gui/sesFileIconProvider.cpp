#include "sesfileiconprovider.h"

#include <QFileIconProvider>
#include <QIcon>

QIcon SesFileIconProvider::icon(const QFileInfo &info) const
{
    QFileIconProvider provider;

    if (info.isDir())
    {
        return QIcon(":/client/theme/ses/ses-folderIconBright.svg");
    }

    if (info.suffix().isEmpty())
    {
        return QIcon(":/client/theme/ses/ses-file.svg");
    }
    
    
    return provider.icon(info);
};