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

#include <koverlayiconplugin.h>
#include <KPluginFactory>
#include <KPluginLoader>
#include <kdebug.h>
#include <kfileitem.h>
#include <QtNetwork/QLocalSocket>


class OwncloudDolphinPlugin : public KOverlayIconPlugin
{
    Q_OBJECT
    QLocalSocket m_socket;
    typedef QHash<QByteArray, QByteArray> StatusMap;
    StatusMap m_status;
    QByteArray m_line;

public:
    explicit OwncloudDolphinPlugin(QObject* parent, const QList<QVariant>&) : KOverlayIconPlugin(parent) {
        connect(&m_socket, SIGNAL(readyRead()), this, SLOT(readyRead()));
        tryConnect();
    }

    virtual QStringList getOverlays(const KFileItem& item) {
        auto url = item.url();
        if (!url.isLocalFile())
            return QStringList();
        const QByteArray localFile = url.toLocalFile().toUtf8();
        kDebug() << localFile;

        tryConnect();
        if (m_socket.state() == QLocalSocket::ConnectingState) {
            if (!m_socket.waitForConnected(100)) {
                kWarning() << "not connected" << m_socket.errorString();
            }
        }
        if (m_socket.state() == QLocalSocket::ConnectedState) {
            m_socket.write("RETRIEVE_FILE_STATUS:");
            m_socket.write(localFile);
            m_socket.write("\n");
        }

        StatusMap::iterator it = m_status.find(localFile);
        if (it != m_status.constEnd()) {
            return  overlaysForString(*it);
        }
        return QStringList();
    }



private:
    void tryConnect() {
        if (m_socket.state() != QLocalSocket::UnconnectedState)
            return;
        QString runtimeDir = QFile::decodeName(qgetenv("XDG_RUNTIME_DIR"));
        QString socketPath = runtimeDir + "/" + "ownCloud" + "/socket";
        m_socket.connectToServer(socketPath);
    }

    QStringList overlaysForString(const QByteArray status) {
        QStringList r;
        if (status.startsWith("NOP"))
            return r;

        if (status.startsWith("OK"))
            r << "dialog-ok";
        if (status.startsWith("SYNC") || status.startsWith("NEW"))
            r << "view-refresh";

        if (status.contains("+SWM"))
            r << "document-share";

        kDebug() << status << r;
        return r;
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
            kDebug() << "got line " << line;
            if (line.isEmpty())
                continue;
            QList<QByteArray> tokens = line.split(':');
            if (tokens.count() != 3)
                continue;
            if (tokens[0] != "STATUS" && tokens[0] != "BROADCAST")
                continue;
            if (tokens[2].isEmpty())
                continue;

            const QByteArray name = tokens[2];
            QByteArray &status = m_status[name]; // reference to the item in the hash
            if (status == tokens[1])
                continue;
            status = tokens[1];

            emit this->overlaysChanged(QUrl::fromLocalFile(QString::fromUtf8(name)), overlaysForString(status));
        }
    }
};

K_PLUGIN_FACTORY(OwncloudDolphinPluginFactory, registerPlugin<OwncloudDolphinPlugin>();)
K_EXPORT_PLUGIN(OwncloudDolphinPluginFactory("ownclouddolhpinplugin"))


#include "ownclouddolphinplugin.moc"
