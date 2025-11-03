// SPDX-FileCopyrightText: 2025 Nextcloud GmbH
// SPDX-FileContributor: Carl Schwan
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "userstatusconnector.h"
#include "clientsideencryptiontokenselector.h"
#include "theme.h"
#include "sharee.h"

#include <QQmlEngine>
#include <qqmlregistration.h>

class UserStatusForeign 
{
    Q_GADGET
    QML_NAMED_ELEMENT(UserStatus)
    QML_FOREIGN(UserStatus)
    QML_UNCREATABLE()
};

class ShareeForeign 
{
    Q_GADGET
    QML_NAMED_ELEMENT(Sharee)
    QML_FOREIGN(Sharee)
    QML_UNCREATABLE()
};

class ClientSideEncryptionTokenSelectorForeign : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(ClientSideEncryptionTokenSelector)
    QML_FOREIGN(ClientSideEncryptionTokenSelector)
    QML_UNCREATABLE()
};

class ThemeForeign : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Theme)
    QML_FOREIGN(Theme)
    QML_SINGLETON
    QML_UNCREATABLE()

    static Theme *create(QQmlEngine *, QJSEngine *engine)
    {
        auto _instance = Theme::instance();
	QQmlEngine::setObjectOwnership(_instance, QJSEngine::CppOwnership);
        return _instance;
    }
};
