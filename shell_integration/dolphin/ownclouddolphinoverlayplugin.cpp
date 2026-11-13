/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "ownclouddolphinpluginhelper.h"
#include <KFileItem>
#include <KOverlayIconPlugin>
#include <KPluginFactory>
#include <QDir>
#include <QIcon>
#include <QTimer>
#include <QtNetwork/QLocalSocket>

using namespace Qt::StringLiterals;

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
        if (!helper->isConnected()) {
            return QStringList();
        }
        if (!url.isLocalFile()) {
            return QStringList();
        }
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
    // The client installs its emblems into the icon theme. Use a theme icon if they are not installed.
    static QString emblem(const QString &name, const QString &fallbackName)
    {
        const auto emblemName = QStringLiteral(APPLICATION_SHORTNAME) + u'_' + name;
        return QIcon::hasThemeIcon(emblemName) ? emblemName : fallbackName;
    }

    QStringList overlaysForString(const QByteArray &status) {
        QStringList r;
        if (status.startsWith("NOP")) {
            return r;
        }

        if (status.contains("+VIRT")) {
            r << emblem(u"dehydrated"_s, u"cloudstatus"_s);
        } else if (status.startsWith("OK")) {
            r << emblem(u"ok"_s, u"vcs-normal"_s);
        }
        if (status.startsWith("SYNC") || status.startsWith("NEW")) {
            r << emblem(u"sync"_s, u"vcs-update-required"_s);
        }
        if (status.startsWith("IGNORE") || status.startsWith("WARN")) {
            r << emblem(u"warn"_s, u"vcs-locally-modified-unstaged"_s);
        }
        if (status.startsWith("ERROR")) {
            r << emblem(u"error"_s, u"vcs-conflicting"_s);
        }

        if (status.contains("+SWM")) {
            r << emblem(u"a_shared"_s, u"emblem-shared"_s);
        }

        return r;
    }

    void slotCommandRecieved(const QByteArray &line) {

        QList<QByteArray> tokens = line.split(':');
        if (tokens.count() < 3) {
            return;
        }
        if (tokens[0] != "STATUS" && tokens[0] != "BROADCAST") {
            return;
        }
        if (tokens[2].isEmpty()) {
            return;
        }

        // We can't use tokens[2] because the filename might contain ':'
        int secondColon = line.indexOf(":", line.indexOf(":") + 1);
        const QByteArray name = line.mid(secondColon + 1);
        QByteArray &status = m_status[name]; // reference to the item in the hash
        if (status == tokens[1]) {
            return;
        }
        status = tokens[1];

        Q_EMIT overlaysChanged(QUrl::fromLocalFile(QString::fromUtf8(name)), overlaysForString(status));
    }
};

#include "ownclouddolphinoverlayplugin.moc"
