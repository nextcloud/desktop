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
#include <QtWidgets/QMenu>
#include <QtCore/QDir>
#include <QtCore/QTimer>
#include "ownclouddolphinpluginhelper.h"

class OwncloudDolphinPluginAction : public KAbstractFileItemActionPlugin
{
    Q_OBJECT
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

        auto menuaction = new QAction(parentWidget);
        menuaction->setText(helper->contextMenuTitle());
        auto menu = new QMenu(parentWidget);
        menuaction->setMenu(menu);

        auto shareAction = menu->addAction(helper->shareActionTitle());
        connect(shareAction, &QAction::triggered, this, [localFile, helper] {
            helper->sendCommand(QByteArray("SHARE:"+localFile.toUtf8()+"\n"));
        } );

        if (!helper->copyPrivateLinkTitle().isEmpty()) {
            auto copyPrivateLinkAction = menu->addAction(helper->copyPrivateLinkTitle());
            connect(copyPrivateLinkAction, &QAction::triggered, this, [localFile, helper] {
                helper->sendCommand(QByteArray("COPY_PRIVATE_LINK:" + localFile.toUtf8() + "\n"));
            });
        }

        if (!helper->emailPrivateLinkTitle().isEmpty()) {
            auto emailPrivateLinkAction = menu->addAction(helper->emailPrivateLinkTitle());
            connect(emailPrivateLinkAction, &QAction::triggered, this, [localFile, helper] {
                helper->sendCommand(QByteArray("EMAIL_PRIVATE_LINK:" + localFile.toUtf8() + "\n"));
            });
        }

        return { menuaction };
    }

};

K_PLUGIN_FACTORY(OwncloudDolphinPluginActionFactory, registerPlugin<OwncloudDolphinPluginAction>();)

#include "ownclouddolphinactionplugin.moc"
