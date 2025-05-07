/*
 * SPDX-FileCopyrightText: 2019 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once
#include <QObject>
#include <QBasicTimer>
#include <QLocalSocket>
#include <QRegularExpression>
#include "ownclouddolphinpluginhelper_export.h"
#include "config.h"

class OWNCLOUDDOLPHINPLUGINHELPER_EXPORT OwncloudDolphinPluginHelper : public QObject {
    Q_OBJECT
public:
    static OwncloudDolphinPluginHelper *instance();

    [[nodiscard]] bool isConnected() const;
    void sendCommand(const char *data);
    [[nodiscard]] QVector<QString> paths() const { return _paths; }

    [[nodiscard]] QString contextMenuTitle() const
    {
        return _strings.value(QStringLiteral("CONTEXT_MENU_TITLE"), QStringLiteral(APPLICATION_NAME));
    }
    [[nodiscard]] QString shareActionTitle() const
    {
        return _strings.value(QStringLiteral("SHARE_MENU_TITLE"), QStringLiteral("Share …"));
    }
    [[nodiscard]] QString contextMenuIconName() const
    {
        return _strings.value(QStringLiteral("CONTEXT_MENU_ICON"), QStringLiteral(APPLICATION_ICON_NAME));
    }

    [[nodiscard]] QString copyPrivateLinkTitle() const { return _strings[QStringLiteral("COPY_PRIVATE_LINK_MENU_TITLE")]; }
    [[nodiscard]] QString emailPrivateLinkTitle() const { return _strings[QStringLiteral("EMAIL_PRIVATE_LINK_MENU_TITLE")]; }

    QByteArray version() { return _version; }

Q_SIGNALS:
    void commandRecieved(const QByteArray &cmd);

protected:
    void timerEvent(QTimerEvent*) override;

private:
    OwncloudDolphinPluginHelper();
    void slotConnected();
    void slotReadyRead();
    void tryConnect();
    QLocalSocket _socket;
    QByteArray _line;
    QVector<QString> _paths;
    QBasicTimer _connectTimer;

    QMap<QString, QString> _strings;
    QByteArray _version;
};
