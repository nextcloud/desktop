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


class Connector : QObject {
    Q_OBJECT
public:
    QLocalSocket m_socket;
    QByteArray m_line;
    QVector<QString> m_paths;
    QString m_shareActionString;

    Connector() {
        connect(&m_socket, SIGNAL(readyRead()), this, SLOT(readyRead()));
        tryConnect();
    }


    void tryConnect() {

        if (m_socket.state() != QLocalSocket::UnconnectedState)
            return;
        QString runtimeDir = QFile::decodeName(qgetenv("XDG_RUNTIME_DIR"));
        QString socketPath = runtimeDir + "/" + "ownCloud" + "/socket";
        m_socket.connectToServer(socketPath);
        if (m_socket.state() == QLocalSocket::ConnectingState) {
            m_socket.waitForConnected(100);
        }
        if (m_socket.state() == QLocalSocket::ConnectedState) {
            m_socket.write("SHARE_MENU_TITLE:\n");
            m_socket.flush();
        }
    }

private slots:
    void readyRead() {
        while (m_socket.bytesAvailable()) {
            m_line += m_socket.readLine();
            if (!m_line.endsWith("\n"))
                continue;
            QByteArray line;
            qSwap(line, m_line);
            line.chop(1);
            if (line.isEmpty())
                continue;
            if (line.startsWith("REGISTER_PATH:")) {
                QString file = QString::fromUtf8(line.mid(line.indexOf(':') + 1));
                m_paths.append(file);
                continue;
            } else if (line.startsWith("SHARE_MENU_TITLE:")) {
                m_shareActionString = QString::fromUtf8(line.mid(line.indexOf(':') + 1));
                continue;
            }
        }
    }
};


class OwncloudDolphinPluginAction : public KAbstractFileItemActionPlugin
{
public:
    explicit OwncloudDolphinPluginAction(QObject* parent, const QList<QVariant>&) : KAbstractFileItemActionPlugin(parent) {
    }

    QList<QAction*> actions(const KFileItemListProperties& fileItemInfos, QWidget* parentWidget) Q_DECL_OVERRIDE
    {
        static Connector connector;
        connector.tryConnect();

        QList<QUrl> urls = fileItemInfos.urlList();
        if (urls.count() != 1 || connector.m_socket.state() != QLocalSocket::ConnectedState)
            return {};

        auto url = urls.first();


        if (!url.isLocalFile())
            return {};
        auto localFile = url.toLocalFile();


        if (!std::any_of(connector.m_paths.begin(), connector.m_paths.end(), [&](const QString &s) {
                                return localFile.startsWith(s);
                        } ))
             return {};

        auto act = new QAction(parentWidget);
        act->setText(connector.m_shareActionString);
        auto socket = &connector.m_socket;
        connect(act, &QAction::triggered, this, [localFile, socket] {
            socket->write("SHARE:");
            socket->write(localFile.toUtf8());
            socket->write("\n");
            socket->flush();
        } );

        return { act };

    }

};

K_PLUGIN_FACTORY(OwncloudDolphinPluginActionFactory, registerPlugin<OwncloudDolphinPluginAction>();)
K_EXPORT_PLUGIN(OwncloudDolphinPluginActionFactory("ownclouddolhpinpluginaction"))

#include "ownclouddolphinpluginaction.moc"
