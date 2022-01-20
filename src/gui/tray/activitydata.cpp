/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include <QtCore>

#include "activitydata.h"
#include "folderman.h"


namespace OCC {

bool operator<(const Activity &rhs, const Activity &lhs)
{
    return rhs._dateTime > lhs._dateTime;
}

bool operator==(const Activity &rhs, const Activity &lhs)
{
    return (rhs._type == lhs._type && rhs._id == lhs._id && rhs._accName == lhs._accName);
}

Activity::Identifier Activity::ident() const
{
    return Identifier(_id, _accName);
}

ActivityLink ActivityLink::createFomJsonObject(const QJsonObject &obj)
{
    ActivityLink activityLink;
    activityLink._label = QUrl::fromPercentEncoding(obj.value(QStringLiteral("label")).toString().toUtf8());
    activityLink._link = obj.value(QStringLiteral("link")).toString();
    activityLink._verb = obj.value(QStringLiteral("type")).toString().toUtf8();
    activityLink._primary = obj.value(QStringLiteral("primary")).toBool();

    return activityLink;
}

OCC::Activity Activity::fromActivityJson(const QJsonObject json, const AccountPtr account)
{
    const auto activityUser = json.value(QStringLiteral("user")).toString();

    Activity activity;
    activity._type = Activity::ActivityType;
    activity._objectType = json.value(QStringLiteral("object_type")).toString();
    activity._objectId = json.value(QStringLiteral("object_id")).toInt();
    activity._objectName = json.value(QStringLiteral("object_name")).toString();
    activity._id = json.value(QStringLiteral("activity_id")).toInt();
    activity._fileAction = json.value(QStringLiteral("type")).toString();
    activity._accName = account->displayName();
    activity._subject = json.value(QStringLiteral("subject")).toString();
    activity._message = json.value(QStringLiteral("message")).toString();
    activity._file = json.value(QStringLiteral("object_name")).toString();
    activity._link = QUrl(json.value(QStringLiteral("link")).toString());
    activity._dateTime = QDateTime::fromString(json.value(QStringLiteral("datetime")).toString(), Qt::ISODate);
    activity._icon = json.value(QStringLiteral("icon")).toString();
    activity._isCurrentUserFileActivity = activity._objectType == QStringLiteral("files") && activityUser == account->davUser();

    auto richSubjectData = json.value(QStringLiteral("subject_rich")).toArray();

    if(richSubjectData.size() > 1) {
        activity._subjectRich = richSubjectData[0].toString();
        auto parameters = richSubjectData[1].toObject();
        const QRegularExpression subjectRichParameterRe(QStringLiteral("({[a-zA-Z0-9]*})"));
        const QRegularExpression subjectRichParameterBracesRe(QStringLiteral("[{}]"));

        for (auto i = parameters.begin(); i != parameters.end(); ++i) {
            const auto parameterJsonObject = i.value().toObject();

            activity._subjectRichParameters[i.key()] = Activity::RichSubjectParameter  {
                parameterJsonObject.value(QStringLiteral("type")).toString(),
                parameterJsonObject.value(QStringLiteral("id")).toString(),
                parameterJsonObject.value(QStringLiteral("name")).toString(),
                parameterJsonObject.contains(QStringLiteral("path")) ? parameterJsonObject.value(QStringLiteral("path")).toString() : QString(),
                parameterJsonObject.contains(QStringLiteral("link")) ? QUrl(parameterJsonObject.value(QStringLiteral("link")).toString()) : QUrl(),
            };
        }

        auto displayString = activity._subjectRich;
        auto subjectRichParameterMatch = subjectRichParameterRe.globalMatch(displayString);

        while (subjectRichParameterMatch.hasNext()) {
            const auto match = subjectRichParameterMatch.next();
            auto word = match.captured(1);
            word.remove(subjectRichParameterBracesRe);

            Q_ASSERT(activity._subjectRichParameters.contains(word));
            displayString = displayString.replace(match.captured(1), activity._subjectRichParameters[word].name);
        }

        activity._subjectDisplay = displayString;
    }

    const auto previewsData = json.value(QStringLiteral("previews")).toArray();

    for(const auto preview : previewsData) {
        const auto jsonPreviewData = preview.toObject();

        PreviewData data;
        data._link = jsonPreviewData.value(QStringLiteral("link")).toString();
        data._mimeType = jsonPreviewData.value(QStringLiteral("mimeType")).toString();
        data._fileId = jsonPreviewData.value(QStringLiteral("fileId")).toInt();
        data._view = jsonPreviewData.value(QStringLiteral("view")).toString();
        data._filename = jsonPreviewData.value(QStringLiteral("filename")).toString();

        if(data._mimeType.contains(QStringLiteral("text/"))) {
            data._source = account->url().toString() + QStringLiteral("/index.php/apps/theming/img/core/filetypes/text.svg");
            data._isMimeTypeIcon = true;
        } else if (data._mimeType.contains(QStringLiteral("/pdf"))) {
            data._source = account->url().toString() + QStringLiteral("/index.php/apps/theming/img/core/filetypes/application-pdf.svg");
            data._isMimeTypeIcon = true;
        } else {
            data._source = jsonPreviewData.value(QStringLiteral("source")).toString();
            data._isMimeTypeIcon = jsonPreviewData.value(QStringLiteral("isMimeTypeIcon")).toBool();
        }

        activity._previews.append(data);
    }

    if(!previewsData.isEmpty()) {
        if(activity._icon.contains(QStringLiteral("add-color.svg"))) {
            activity._icon = "qrc:///client/theme/colored/add-bordered.svg";
        } else if(activity._icon.contains(QStringLiteral("delete-color.svg"))) {
            activity._icon = "qrc:///client/theme/colored/delete-bordered.svg";
        } else if(activity._icon.contains(QStringLiteral("change.svg"))) {
            activity._icon = "qrc:///client/theme/colored/change-bordered.svg";
        }
    }

    return activity;
}

}
