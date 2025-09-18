/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2016 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QtCore>

#include "activitydata.h"
#include "folderman.h"

namespace {
QUrl stringToUrl(const QUrl &accountUrl, const QString &link) {
    auto url = QUrl::fromUserInput(link);

    if (!url.isValid()) {
        return {};
    }

    if (url.host().isEmpty()) {
        url.setScheme(accountUrl.scheme());
        url.setHost(accountUrl.host());
    }

    if (url.port() == -1) {
        url.setPort(accountUrl.port());
    }

    return url;
};
}

namespace OCC {

bool operator<(const Activity &rhs, const Activity &lhs)
{
    return rhs._dateTime > lhs._dateTime;
}

bool operator>(const Activity &rhs, const Activity &lhs)
{
    return rhs._dateTime < lhs._dateTime;
}

bool operator==(const Activity &rhs, const Activity &lhs)
{
    return (rhs._type == lhs._type && rhs._id == lhs._id && rhs._accName == lhs._accName);
}

bool operator!=(const Activity &rhs, const Activity &lhs)
{
    return !(rhs == lhs);
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
    activity._link = stringToUrl(account->url(), json.value(QStringLiteral("link")).toString());
    activity._dateTime = QDateTime::fromString(json.value(QStringLiteral("datetime")).toString(), Qt::ISODate);
    activity._icon = json.value(QStringLiteral("icon")).toString();
    activity._isCurrentUserFileActivity = activity._objectType == QStringLiteral("files") && activityUser == account->davUser();
    activity._isMultiObjectActivity = json.value("objects").toObject().count() > 1;

    auto richSubjectData = json.value(QStringLiteral("subject_rich")).toArray();

    if(richSubjectData.size() > 1) {
        activity._subjectRich = richSubjectData[0].toString();
        auto parameters = richSubjectData[1].toObject();
        const QRegularExpression subjectRichParameterRe(QStringLiteral("({[a-zA-Z0-9]*})"));
        const QRegularExpression subjectRichParameterBracesRe(QStringLiteral("[{}]"));

        for (auto i = parameters.begin(); i != parameters.end(); ++i) {
            const auto parameterJsonObject = i.value().toObject();

            const auto richParamLink = stringToUrl(account->url(), parameterJsonObject.value(QStringLiteral("link")).toString());
            activity._subjectRichParameters[i.key()] = QVariant::fromValue(Activity::RichSubjectParameter{
                parameterJsonObject.value(QStringLiteral("type")).toString(),
                parameterJsonObject.value(QStringLiteral("id")).toString(),
                parameterJsonObject.value(QStringLiteral("name")).toString(),
                parameterJsonObject.contains(QStringLiteral("path")) ? parameterJsonObject.value(QStringLiteral("path")).toString() : QString(),
                richParamLink,
            });

            if (activity._objectType == QStringLiteral("calendar") && activity._link.isEmpty()) {
                activity._link = richParamLink;
            }
        }

        auto displayString = activity._subjectRich;
        auto subjectRichParameterMatch = subjectRichParameterRe.globalMatch(displayString);

        while (subjectRichParameterMatch.hasNext()) {
            const auto match = subjectRichParameterMatch.next();
            auto word = match.captured(1);
            word.remove(subjectRichParameterBracesRe);

            displayString = displayString.replace(match.captured(1), activity._subjectRichParameters[word].value<Activity::RichSubjectParameter>().name);
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
        if(activity._icon.contains(QStringLiteral("add-color.svg"))) {
            activity._icon = "image://svgimage-custom-color/add.svg";
        } else if(activity._icon.contains(QStringLiteral("delete.svg"))) {
            activity._icon = "image://svgimage-custom-color/delete.svg";
        } else if(activity._icon.contains(QStringLiteral("change.svg"))) {
            activity._icon = "image://svgimage-custom-color/change.svg";
        }
    }

    auto actions = json.value("actions").toArray();
    for (const auto &action : actions) {
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
