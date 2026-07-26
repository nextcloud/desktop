/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QString>
#include <QUrl>

namespace OCC {

class UriSchemeHandler
{
public:
    enum class Action {
        Invalid,
        OpenLocalEdit,
        Login,
    };

    struct ParsedUri {
        Action action = Action::Invalid;
        QUrl originalUrl;
        QUrl serverUrl;
        QString error;
    };

    [[nodiscard]] static ParsedUri parseUri(const QUrl &url);
    static bool handleUri(const QUrl &url);
};

}
