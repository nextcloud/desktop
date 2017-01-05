/******************************************************************************
 *   Copyright (C) 2014 by Olivier Goffart <ogoffart@woboq.com                *
 *                                                                            *
 *   This program is free software; you can redistribute it and/or modify     *
 *   it under the terms of the GNU General Public License as published by     *
 *   the Free Software Foundation; either version 2 of the License, or        *
 *   (at your option) any later version.                                      *
 *                                                                            *
 *   This program is distributed in the hope that it will be useful,          *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of           *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            *
 *   GNU General Public License for more details.                             *
 *                                                                            *
 *   You should have received a copy of the GNU General Public License        *
 *   along with this program; if not, write to the                            *
 *   Free Software Foundation, Inc.,                                          *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA               *
 ******************************************************************************/

#include <KPluginFactory>
#include <KPluginLoader>
#include <KIOWidgets/kabstractfileitemactionplugin.h>
#include <QtNetwork/QLocalSocket>
#include <KIOCore/kfileitem.h>
#include <KIOCore/KFileItemListProperties>
#include <QtWidgets/QAction>
#include <QtCore/QDir>
#include <QtCore/QTimer>
#include "ownclouddolphinpluginhelper.h"

class OwncloudDolphinPluginAction : public KAbstractFileItemActionPlugin
{
public:
    explicit OwncloudDolphinPluginAction(QObject* parent, const QList<QVariant>&)
        : KAbstractFileItemActionPlugin(parent) { }

    QList<QAction*> actions(const KFileItemListProperties& fileItemInfos, QWidget* parentWidget) Q_DECL_OVERRIDE
    {
        auto helper = OwncloudDolphinPluginHelper::instance();
        QList<QUrl> urls = fileItemInfos.urlList();
        if (urls.count() != 1 || !helper->isConnected())
            return {};

        auto url = urls.first();
        if (!url.isLocalFile())
            return {};
        QDir localPath(url.toLocalFile());
        auto localFile = localPath.canonicalPath();

        const auto paths = helper->paths();
        if (!std::any_of(paths.begin(), paths.end(), [&](const QString &s) {
                                return localFile.startsWith(s);
                        } ))
             return {};

        auto act = new QAction(parentWidget);
        act->setText(helper->shareActionString());
        connect(act, &QAction::triggered, this, [localFile, helper] {
            helper->sendCommand(QByteArray("SHARE:"+localFile.toUtf8()+"\n"));
        } );
        return { act };
    }

};

K_PLUGIN_FACTORY(OwncloudDolphinPluginActionFactory, registerPlugin<OwncloudDolphinPluginAction>();)

#include "ownclouddolphinactionplugin.moc"
