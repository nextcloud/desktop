/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "searchresponses.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>

namespace OCC::DocAssets
{
namespace
{
QByteArray ocs(const QJsonValue &data)
{
    return QJsonDocument(QJsonObject{{"ocs", QJsonObject{{"meta", QJsonObject{{"status", "ok"}, {"statuscode", 200}}}, {"data", data}}}})
        .toJson(QJsonDocument::Compact);
}
}
QByteArray searchResponse(const QUrl &url)
{
    if (url.scheme() != QStringLiteral("https") || url.host() != QStringLiteral("cloud.example.com")) {
        return {};
    }
    const auto path = url.path();
    if (path == QStringLiteral("/ocs/v2.php/search/providers")) {
        const QJsonArray filters{"term", "since", "until", "person"};
        return ocs(QJsonArray{
            QJsonObject{{"id", "files"}, {"appId", "files"}, {"name", "Files"}, {"icon", "icon-folder"}, {"order", 0}, {"filters", filters}},
            QJsonObject{{"id", "talk-message"}, {"appId", "spreed"}, {"name", "Messages"}, {"icon", "icon-comment"}, {"order", 1}, {"filters", filters}},
        });
    }
    if (path == QStringLiteral("/ocs/v2.php/search/providers/files/search") || path == QStringLiteral("/ocs/v2.php/search/providers/talk-message/search")) {
        if (QUrlQuery(url).queryItemValue(QStringLiteral("term")) != QStringLiteral("Project")) {
            return {};
        }
        const auto files = path.contains(QStringLiteral("/files/"));
        QJsonArray entries;
        const QStringList titles = files
            ? QStringList{QStringLiteral("Project plan.pdf"), QStringLiteral("Project budget.ods"), QStringLiteral("Project notes.md")}
            : QStringList{QStringLiteral("Project team"), QStringLiteral("Project launch")};
        for (auto index = 0; index < titles.size(); ++index) {
            entries.append(QJsonObject{
                {"title", titles.at(index)},
                {"subline", files ? "Documents / Project" : "Jamie Rivera: The project review is ready."},
                {"resourceUrl", QStringLiteral("https://cloud.example.com/%1/%2").arg(files ? QStringLiteral("f") : QStringLiteral("call")).arg(index + 1)},
                {"icon", files ? "icon-file" : "icon-comment"},
                {"thumbnailUrl", ""},
                {"rounded", false},
            });
        }
        return ocs(QJsonObject{{"entries", entries}, {"isPaginated", false}});
    }
    if (path == QStringLiteral("/ocs/v2.php/apps/files_sharing/api/v1/sharees")) {
        const QUrlQuery query(url);
        if (query.queryItemValue(QStringLiteral("search")) != QStringLiteral("Jamie")
            || query.queryItemValue(QStringLiteral("itemType")) != QStringLiteral("file")
            || query.queryItemValue(QStringLiteral("shareType")) != QStringLiteral("0")) {
            return {};
        }
        return ocs(QJsonObject{{"users",
                                QJsonArray{
                                    QJsonObject{{"label", "Jamie Rivera"}, {"value", QJsonObject{{"shareWith", "jamie"}}}},
                                    QJsonObject{{"label", "Alex Morgan"}, {"value", QJsonObject{{"shareWith", "alex"}}}},
                                }}});
    }
    return {};
}
}
