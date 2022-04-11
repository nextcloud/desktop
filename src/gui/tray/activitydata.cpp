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

OCC::Activity Activity::fromActivityJson(const QJsonObject &json, const AccountPtr account)
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
    activity._darkIcon = json.value(QStringLiteral("icon")).toString();  // We have both dark and light for theming purposes
    activity._lightIcon = json.value(QStringLiteral("icon")).toString(); // Some icons get changed in the ActivityListModel
    activity._isCurrentUserFileActivity = activity._objectType == QStringLiteral("files") && activityUser == account->davUser();

    const auto darkIconPath = QStringLiteral("qrc://:/client/theme/white/");
    const auto lightIconPath = QStringLiteral("qrc://:/client/theme/black/");
    if(activity._darkIcon.contains("change.svg")) {
        activity._darkIcon = darkIconPath + QStringLiteral("change.svg");
        activity._lightIcon = lightIconPath + QStringLiteral("change.svg");
    } else if(activity._darkIcon.contains("calendar.svg")) {
        activity._darkIcon = darkIconPath + QStringLiteral("calendar.svg");
        activity._lightIcon = lightIconPath + QStringLiteral("calendar.svg");
    } else if(activity._darkIcon.contains("personal.svg")) {
        activity._darkIcon = darkIconPath + QStringLiteral("user.svg");
        activity._lightIcon = lightIconPath + QStringLiteral("user.svg");
    }  else if(activity._darkIcon.contains("core/img/actions")) {
        activity._darkIcon.insert(activity._darkIcon.indexOf(".svg"), "-white");
    }

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
    const QMimeDatabase mimeDb;

    for(const auto &preview : previewsData) {
        const auto jsonPreviewData = preview.toObject();

        PreviewData data;
        data._link = jsonPreviewData.value(QStringLiteral("link")).toString();
        data._mimeType = jsonPreviewData.value(QStringLiteral("mimeType")).toString();
        data._fileId = jsonPreviewData.value(QStringLiteral("fileId")).toInt();
        data._view = jsonPreviewData.value(QStringLiteral("view")).toString();
        data._filename = jsonPreviewData.value(QStringLiteral("filename")).toString();

        const auto mimeType = mimeDb.mimeTypeForName(data._mimeType);

        if(data._mimeType.contains(QStringLiteral("text/")) || data._mimeType.contains(QStringLiteral("/pdf"))) {
            data._source = account->url().toString() + relativeServerFileTypeIconPath(mimeType);
            data._isMimeTypeIcon = true;
        } else {
            data._source = jsonPreviewData.value(QStringLiteral("source")).toString();
            data._isMimeTypeIcon = jsonPreviewData.value(QStringLiteral("isMimeTypeIcon")).toBool();
        }

        activity._previews.append(data);
    }

    if(!previewsData.isEmpty()) {
        if(activity._darkIcon.contains(QStringLiteral("add-color.svg"))) {
            activity._darkIcon = "qrc:///client/theme/colored/add-bordered.svg";
            activity._lightIcon = "qrc:///client/theme/colored/add-bordered.svg";
        } else if(activity._darkIcon.contains(QStringLiteral("delete-color.svg"))) {
            activity._darkIcon = "qrc:///client/theme/colored/delete-bordered.svg";
            activity._lightIcon = "qrc:///client/theme/colored/add-bordered.svg";
        } else if(activity._darkIcon.contains(QStringLiteral("change.svg"))) {
            activity._darkIcon = "qrc:///client/theme/colored/change-bordered.svg";
            activity._lightIcon = "qrc:///client/theme/colored/add-bordered.svg";
        }
    }

    auto actions = json.value("actions").toArray();
    foreach (auto action, actions) {
        activity._links.append(ActivityLink::createFomJsonObject(action.toObject()));
    }

    return activity;
}

QString Activity::relativeServerFileTypeIconPath(const QMimeType &mimeType)
{
    if(mimeType.isValid() && mimeType.inherits("text/plain")) {
        return QStringLiteral("/index.php/apps/theming/img/core/filetypes/text.svg");
    } else if (mimeType.isValid() && mimeType.name().startsWith("image")) {
        return QStringLiteral("/index.php/apps/theming/img/core/filetypes/image.svg");
    } else if (mimeType.isValid() && mimeType.name().startsWith("audio")) {
        return QStringLiteral("/index.php/apps/theming/img/core/filetypes/audio.svg");
    } else if (mimeType.isValid() && mimeType.name().startsWith("video")) {
        return QStringLiteral("/index.php/apps/theming/img/core/filetypes/video.svg");
    } else if (mimeType.isValid() && (mimeType.inherits("application/vnd.oasis.opendocument.text") ||
                                      mimeType.inherits("application/msword") ||
                                      mimeType.inherits("application/vnd.openxmlformats-officedocument.wordprocessingml.document") ||
                                      mimeType.inherits("application/vnd.openxmlformats-officedocument.wordprocessingml.template")||
                                      mimeType.inherits("application/vnd.ms-word.document.macroEnabled.12") ||
                                      mimeType.inherits("application/vnd.ms-word.template.macroEnabled.12") ||
                                      mimeType.inherits("application/vnd.apple.pages"))) {
        return QStringLiteral("/index.php/apps/theming/img/core/filetypes/x-office-document.svg");
    } else if (mimeType.isValid() && mimeType.inherits("application/vnd.oasis.opendocument.graphics")) {
        return QStringLiteral("/index.php/apps/theming/img/core/filetypes/x-office-drawing.svg");
    } else if (mimeType.isValid() && (mimeType.inherits("application/vnd.oasis.opendocument.presentation") ||
                                      mimeType.inherits("application/vnd.ms-powerpoint") ||
                                      mimeType.inherits("application/vnd.openxmlformats-officedocument.presentationml.presentation") ||
                                      mimeType.inherits("application/vnd.openxmlformats-officedocument.presentationml.template") ||
                                      mimeType.inherits("application/vnd.openxmlformats-officedocument.presentationml.slideshow") ||
                                      mimeType.inherits("application/vnd.ms-powerpoint.addin.macroEnabled.12") ||
                                      mimeType.inherits("application/vnd.ms-powerpoint.presentation.macroEnabled.12") ||
                                      mimeType.inherits("application/vnd.ms-powerpoint.template.macroEnabled.12") ||
                                      mimeType.inherits("application/vnd.ms-powerpoint.slideshow.macroEnabled.12") ||
                                      mimeType.inherits("application/vnd.apple.keynote"))) {
        return QStringLiteral("/index.php/apps/theming/img/core/filetypes/x-office-presentation.svg");
    } else if (mimeType.isValid() && (mimeType.inherits("application/vnd.oasis.opendocument.spreadsheet") ||
                                      mimeType.inherits("application/vnd.ms-excel") ||
                                      mimeType.inherits("application/vnd.openxmlformats-officedocument.spreadsheetml.sheet") ||
                                      mimeType.inherits("application/vnd.openxmlformats-officedocument.spreadsheetml.template") ||
                                      mimeType.inherits("application/vnd.ms-excel.sheet.macroEnabled.12") ||
                                      mimeType.inherits("application/vnd.ms-excel.template.macroEnabled.12") ||
                                      mimeType.inherits("application/vnd.ms-excel.addin.macroEnabled.12") ||
                                      mimeType.inherits("application/vnd.ms-excel.sheet.binary.macroEnabled.12") ||
                                      mimeType.inherits("application/vnd.apple.numbers"))) {
        return QStringLiteral("/index.php/apps/theming/img/core/filetypes/x-office-document.svg");
    } else if (mimeType.isValid() && mimeType.inherits("application/pdf")) {
        return QStringLiteral("/index.php/apps/theming/img/core/filetypes/application-pdf.svg");
    } else {
        return QStringLiteral("/index.php/apps/theming/img/core/filetypes/file.svg");
    }
}

}
