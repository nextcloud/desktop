/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "screenshotuser.h"

namespace OCC {

ScreenshotUser::ScreenshotUser(QObject *parent)
    : QObject(parent)
{
}

QString ScreenshotUser::name() const
{
    return QStringLiteral("Alex Morgan");
}

QString ScreenshotUser::server() const
{
    return QStringLiteral("cloud.example.com");
}

QString ScreenshotUser::avatar() const
{
    return QStringLiteral("qrc:/client/theme/colored/Nextcloud-icon-128.png");
}

QColor ScreenshotUser::accentColor() const
{
    return QColor(QStringLiteral("#0082c9"));
}

QColor ScreenshotUser::headerColor() const
{
    return accentColor();
}

QColor ScreenshotUser::headerTextColor() const
{
    return QColor(Qt::white);
}

bool ScreenshotUser::hasLocalFolder() const
{
    return true;
}

bool ScreenshotUser::hasFileProvider() const
{
    return true;
}

bool ScreenshotUser::isConnected() const
{
    return true;
}

bool ScreenshotUser::needsToSignTermsOfService() const
{
    return false;
}

bool ScreenshotUser::isAssistantEnabled() const
{
    return true;
}

QString ScreenshotUser::assistantResponse() const
{
    return {};
}

QString ScreenshotUser::assistantError() const
{
    return {};
}

QVariantList ScreenshotUser::assistantMessages() const
{
    return {
        QVariantMap{
            {QStringLiteral("role"), QStringLiteral("user")},
            {QStringLiteral("text"), QStringLiteral("Summarize the project notes from this week.")},
        },
        QVariantMap{
            {QStringLiteral("role"), QStringLiteral("assistant")},
            {QStringLiteral("text"), QStringLiteral("The team completed the design review and prepared the next milestone.")},
        },
    };
}

bool ScreenshotUser::assistantRequestInProgress() const
{
    return false;
}

void ScreenshotUser::forceSyncNow()
{
}

void ScreenshotUser::openServer()
{
}

void ScreenshotUser::submitAssistantQuestion(const QString &question)
{
    Q_UNUSED(question)
}

void ScreenshotUser::clearAssistantResponse()
{
}

}
