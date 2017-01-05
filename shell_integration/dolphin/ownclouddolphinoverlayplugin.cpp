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

#include <KOverlayIconPlugin>
#include <KPluginFactory>
#include <QtNetwork/QLocalSocket>
#include <KIOCore/kfileitem.h>
#include <QDir>
#include <QTimer>
#include "ownclouddolphinpluginhelper.h"

class OwncloudDolphinPlugin : public KOverlayIconPlugin
{
    Q_PLUGIN_METADATA(IID "com.owncloud.ovarlayiconplugin" FILE "ownclouddolphinoverlayplugin.json")
    Q_OBJECT

    typedef QHash<QByteArray, QByteArray> StatusMap;
    StatusMap m_status;

public:

    OwncloudDolphinPlugin() {
        auto helper = OwncloudDolphinPluginHelper::instance();
        QObject::connect(helper, &OwncloudDolphinPluginHelper::commandRecieved,
                         this, &OwncloudDolphinPlugin::slotCommandRecieved);
    }

    QStringList getOverlays(const QUrl& url) override {
        auto helper = OwncloudDolphinPluginHelper::instance();
        if (!helper->isConnected())
            return QStringList();
        if (!url.isLocalFile())
            return QStringList();
        QDir localPath(url.toLocalFile());
        const QByteArray localFile = localPath.canonicalPath().toUtf8();

        helper->sendCommand(QByteArray("RETRIEVE_FILE_STATUS:" + localFile + "\n"));

        StatusMap::iterator it = m_status.find(localFile);
        if (it != m_status.constEnd()) {
            return  overlaysForString(*it);
        }
        return QStringList();
    }

private:
    QStringList overlaysForString(const QByteArray &status) {
        QStringList r;
        if (status.startsWith("NOP"))
            return r;

        if (status.startsWith("OK"))
            r << "vcs-normal";
        if (status.startsWith("SYNC") || status.startsWith("NEW"))
            r << "vcs-update-required";
        if (status.startsWith("IGNORE") || status.startsWith("WARN"))
            r << "vcs-locally-modified-unstaged";
        if (status.startsWith("ERROR"))
            r << "vcs-conflicting";

        if (status.contains("+SWM"))
            r << "document-share";

        return r;
    }

    void slotCommandRecieved(const QByteArray &line) {

        QList<QByteArray> tokens = line.split(':');
        if (tokens.count() != 3)
            return;
        if (tokens[0] != "STATUS" && tokens[0] != "BROADCAST")
            return;
        if (tokens[2].isEmpty())
            return;

        const QByteArray name = tokens[2];
        QByteArray &status = m_status[name]; // reference to the item in the hash
        if (status == tokens[1])
            return;
        status = tokens[1];

        emit overlaysChanged(QUrl::fromLocalFile(QString::fromUtf8(name)), overlaysForString(status));
    }
};

#include "ownclouddolphinoverlayplugin.moc"
