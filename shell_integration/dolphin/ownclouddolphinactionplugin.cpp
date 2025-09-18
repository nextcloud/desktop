/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <KPluginFactory>
#include <KAbstractFileItemActionPlugin>
#include <QtNetwork/QLocalSocket>
#include <KFileItem>
#include <KFileItemListProperties>
#include <QAction>
#include <QMenu>
#include <QDir>
#include <QTimer>
#include <QEventLoop>
#include "ownclouddolphinpluginhelper.h"

class OwncloudDolphinPluginAction : public KAbstractFileItemActionPlugin
{
    Q_OBJECT
public:
    explicit OwncloudDolphinPluginAction(QObject* parent, const QList<QVariant>&)
        : KAbstractFileItemActionPlugin(parent) { }

    QList<QAction*> actions(const KFileItemListProperties& fileItemInfos, QWidget* parentWidget) override
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
                    helper->sendCommand(QByteArray(call + ":" + files + "\n").constData());
                });
            }
        });
        QTimer::singleShot(100, &loop, &QEventLoop::quit); // add a timeout to be sure we don't freeze dolphin
        helper->sendCommand(QByteArray("GET_MENU_ITEMS:" + files + "\n").constData());
        loop.exec(QEventLoop::ExcludeUserInputEvents);
        disconnect(con);
        if (menu->actions().isEmpty()) {
            delete menu;
            return {};
        }
        
        menu->setTitle(helper->contextMenuTitle());
        menu->setIcon(QIcon::fromTheme(helper->contextMenuIconName()));
        return { menu->menuAction() };
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
            helper->sendCommand(QByteArray("SHARE:" + localFile.toUtf8() + "\n").constData());
        });

        if (!helper->copyPrivateLinkTitle().isEmpty()) {
            auto copyPrivateLinkAction = menu->addAction(helper->copyPrivateLinkTitle());
            connect(copyPrivateLinkAction, &QAction::triggered, this, [localFile, helper] {
                helper->sendCommand(QByteArray("COPY_PRIVATE_LINK:" + localFile.toUtf8() + "\n").constData());
            });
        }

        if (!helper->emailPrivateLinkTitle().isEmpty()) {
            auto emailPrivateLinkAction = menu->addAction(helper->emailPrivateLinkTitle());
            connect(emailPrivateLinkAction, &QAction::triggered, this, [localFile, helper] {
                helper->sendCommand(QByteArray("EMAIL_PRIVATE_LINK:" + localFile.toUtf8() + "\n").constData());
            });
        }
        return { menuaction };
    }

};

K_PLUGIN_CLASS_WITH_JSON(OwncloudDolphinPluginAction, APPLICATION_EXECUTABLE "dolphinactionplugin.json")

#include "ownclouddolphinactionplugin.moc"
