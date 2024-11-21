#include "sesfileiconprovider.h"

#include <QFileIconProvider>
#include <QIcon>

QIcon SesFileIconProvider::icon(const QFileInfo &info) const
{
    QFileIconProvider provider;

    if (info.isDir())
    {
        return QIcon(":/client/theme/black/folder.svg");
    }

    if (info.suffix().isEmpty())
    {
        return QIcon(":/client/theme/black/edit.svg");
    }
    
    
    return provider.icon(info);
};