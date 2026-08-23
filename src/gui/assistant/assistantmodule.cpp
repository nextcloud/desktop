/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "assistantmodule.h"

#include "account.h"
#include "accountstate.h"
#include "assistantcontroller.h"
#include "theme.h"

#include <QLoggingCategory>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QVariantMap>
#include <qqml.h>

#include <memory>

static void initializeAssistantResources()
{
    Q_INIT_RESOURCE(assistant);
}

namespace OCC::AssistantModule {

namespace {

Q_LOGGING_CATEGORY(lcAssistantModule, "nextcloud.gui.assistant.module", QtInfoMsg)

QVariantMap accountHeaderProperties(const AccountPtr &account)
{
    auto serverUrl = account->url();
    if (account->isPublicShareLink()) {
        serverUrl.setUserName({});
    }

    auto server = serverUrl.toString();
    server.remove(QStringLiteral("https://"));
    server.remove(QStringLiteral("http://"));

    auto avatar = QString{};
#ifndef TOKEN_AUTH_ONLY
    if (!account->avatar().isNull()) {
        avatar = QStringLiteral("image://avatars/") + account->id();
    }
#endif

    return {
        {QStringLiteral("name"), account->prettyName()},
        {QStringLiteral("server"), server},
        {QStringLiteral("avatar"), avatar},
    };
}

}

void registerQmlTypes()
{
    static const auto initialized = [] {
        initializeAssistantResources();
        qmlRegisterUncreatableType<AssistantController>(
            "com.nextcloud.desktopclient", 1, 0, "AssistantController", "Owned by the Assistant window");
        return true;
    }();
    Q_UNUSED(initialized)
}

QQuickWindow *createWindow(QQmlEngine *engine, const AccountStatePtr &accountState)
{
    if (!engine || !accountState || !accountState->account()) {
        qCWarning(lcAssistantModule) << "Cannot create an Assistant window without an engine and account.";
        return nullptr;
    }

    registerQmlTypes();
    QQmlComponent component(engine, QStringLiteral("qrc:/qml/src/gui/assistant/qml/AssistantWindow.qml"));
    if (component.isError()) {
        qCWarning(lcAssistantModule) << component.errorString();
        qCWarning(lcAssistantModule) << component.errors();
        return nullptr;
    }

    auto controller = std::make_unique<AssistantController>(accountState);
    const auto accountHeader = accountHeaderProperties(accountState->account());
    const QVariantMap initialProperties{
        {QStringLiteral("accountName"), accountHeader.value(QStringLiteral("name"))},
        {QStringLiteral("accountServer"), accountHeader.value(QStringLiteral("server"))},
        {QStringLiteral("accountAvatar"), accountHeader.value(QStringLiteral("avatar"))},
        {QStringLiteral("assistantController"), QVariant::fromValue(controller.get())},
    };

    const auto createdObject = component.createWithInitialProperties(initialProperties);
    const auto window = qobject_cast<QQuickWindow *>(createdObject);
    if (!window) {
        qCWarning(lcAssistantModule) << "Assistant component did not create a window.";
        if (createdObject) {
            controller.release()->setParent(createdObject);
            createdObject->deleteLater();
        }
        return nullptr;
    }

    controller.release()->setParent(window);
    window->setIcon(Theme::instance()->applicationIcon());
    return window;
}

}
