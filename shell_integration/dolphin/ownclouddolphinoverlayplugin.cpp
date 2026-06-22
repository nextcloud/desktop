/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <KOverlayIconPlugin>
#include <KPluginFactory>
#include <QtNetwork/QLocalSocket>
#include <KFileItem>
#include <QDir>
#include <QTimer>
#include "ownclouddolphinpluginhelper.h"

class OwncloudDolphinPlugin : public KOverlayIconPlugin
{
    Q_PLUGIN_METADATA(IID "com.owncloud.ovarlayiconplugin" FILE "ownclouddolphinoverlayplugin.json")
    Q_OBJECT

    using StatusMap = QHash<QByteArray, QByteArray>;
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

        helper->sendCommand(QByteArray("RETRIEVE_FILE_STATUS:" + localFile + "\n").constData());

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
            r << QStringLiteral("vcs-normal");
        if (status.startsWith("SYNC") || status.startsWith("NEW"))
            r << QStringLiteral("vcs-update-required");
        if (status.startsWith("IGNORE") || status.startsWith("WARN"))
            r << QStringLiteral("vcs-locally-modified-unstaged");
        if (status.startsWith("ERROR"))
            r << QStringLiteral("vcs-conflicting");

        if (status.contains("+SWM"))
            r << QStringLiteral("document-share");

        return r;
    }

    void slotCommandRecieved(const QByteArray &line) {

        QList<QByteArray> tokens = line.split(':');
        if (tokens.count() < 3)
            return;
        if (tokens[0] != "STATUS" && tokens[0] != "BROADCAST")
            return;
        if (tokens[2].isEmpty())
            return;

        // We can't use tokens[2] because the filename might contain ':'
        int secondColon = line.indexOf(":", line.indexOf(":") + 1);
        const QByteArray name = line.mid(secondColon + 1);
        QByteArray &status = m_status[name]; // reference to the item in the hash
        if (status == tokens[1])
            return;
        status = tokens[1];

        Q_EMIT overlaysChanged(QUrl::fromLocalFile(QString::fromUtf8(name)), overlaysForString(status));
    }
};

#include "ownclouddolphinoverlayplugin.moc"
