#include "sesFileIconProvider.h"
#include "whitelabeltheme.h"

#include <QFileIconProvider>
#include <QIcon>

QIcon SesFileIconProvider::icon(const QFileInfo &info) const
{
    QFileIconProvider provider;

    if (info.isDir())
    {
        return QIcon(OCC::WLTheme.folderIcon("qtwidget"));
    }

    if (info.suffix().isEmpty())
    {
        return QIcon(":/client/theme/ses/ses-file.svg");
    }
    
    
    return provider.icon(info);
};