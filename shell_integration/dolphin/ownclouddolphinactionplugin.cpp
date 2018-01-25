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

#include <KCoreAddons/KPluginFactory>
#include <KCoreAddons/KPluginLoader>
#include <KIOWidgets/kabstractfileitemactionplugin.h>
#include <QtNetwork/QLocalSocket>
#include <KIOCore/kfileitem.h>
#include <KIOCore/KFileItemListProperties>
#include <QtWidgets/QAction>
#include <QtWidgets/QMenu>
#include <QtCore/QDir>
#include <QtCore/QTimer>
#include <QtCore/QEventLoop>
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
        if (!helper->isConnected() || !fileItemInfos.isLocal())
            return {};

        // If any of the url is outside of a sync folder, return an empty menu.
        const QList<QUrl> urls = fileItemInfos.urlList();
        const auto paths = helper->paths();
        QByteArray files;
        for (const auto &url : urls) {
            QDir localPath(url.toLocalFile());
            auto localFile = localPath.canonicalPath();
            if (!std::any_of(paths.begin(), paths.end(), [&](const QString &s) {
                    return localFile.startsWith(s);
                }))
                return {};

            if (!files.isEmpty())
                files += '\x1e'; // Record separator
            files += localFile.toUtf8();
        }

        if (helper->version() < "1.1") { // in this case, lexicographic order works
            return legacyActions(fileItemInfos, parentWidget);
        }

        auto menu = new QMenu(parentWidget);
        QEventLoop loop;
        auto con = connect(helper, &OwncloudDolphinPluginHelper::commandRecieved, this, [&](const QByteArray &cmd) {
            if (cmd.startsWith("GET_MENU_ITEMS:END")) {
                loop.quit();
            } else if (cmd.startsWith("MENU_ITEM:")) {
                auto args = QString::fromUtf8(cmd).split(QLatin1Char(':'));
                if (args.size() < 4)
                    return;
                auto action = menu->addAction(args.mid(3).join(QLatin1Char(':')));
                if (args.value(2).contains(QLatin1Char('d')))
                    action->setDisabled(true);
                auto call = args.value(1).toLatin1();
                connect(action, &QAction::triggered, [helper, call, files] {
                    helper->sendCommand(QByteArray(call + ":" + files + "\n"));
                });
            }
        });
        QTimer::singleShot(100, &loop, SLOT(quit())); // add a timeout to be sure we don't freeze dolphin
        helper->sendCommand(QByteArray("GET_MENU_ITEMS:" + files + "\n"));
        loop.exec(QEventLoop::ExcludeUserInputEvents);
        disconnect(con);
        if (menu->actions().isEmpty()) {
            delete menu;
            return {};
        }

        auto menuaction = new QAction(parentWidget);
        menuaction->setText(helper->contextMenuTitle());
        menuaction->setMenu(menu);
        return { menuaction };
    }


    QList<QAction *> legacyActions(const KFileItemListProperties &fileItemInfos, QWidget *parentWidget)
    {
        QList<QUrl> urls = fileItemInfos.urlList();
        if (urls.count() != 1)
            return {};
        QDir localPath(urls.first().toLocalFile());
        auto localFile = localPath.canonicalPath();
        auto helper = OwncloudDolphinPluginHelper::instance();
        auto menuaction = new QAction(parentWidget);
        menuaction->setText(helper->contextMenuTitle());
        auto menu = new QMenu(parentWidget);
        menuaction->setMenu(menu);

        auto shareAction = menu->addAction(helper->shareActionTitle());
        connect(shareAction, &QAction::triggered, this, [localFile, helper] {
            helper->sendCommand(QByteArray("SHARE:" + localFile.toUtf8() + "\n"));
        });

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
