/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2016 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QtCore>

#include "activitydata.h"
#include "folderman.h"

using namespace Qt::StringLiterals;

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
    activityLink._label = QUrl::fromPercentEncoding(obj.value("label"_L1).toString().toUtf8());
    activityLink._link = obj.value("link"_L1).toString();
    activityLink._verb = obj.value("type"_L1).toString().toUtf8();
    activityLink._primary = obj.value("primary"_L1).toBool();

    return activityLink;
}

OCC::Activity Activity::fromActivityJson(const QJsonObject &json, const AccountPtr account)
{
    const auto activityUser = json.value("user"_L1).toString();

    Activity activity;
    activity._type = Activity::ActivityType;
    activity._objectType = json.value("object_type"_L1).toString();
    activity._objectId = json.value("object_id"_L1).toInt();
    activity._objectName = json.value("object_name"_L1).toString();
    activity._id = json.value("activity_id"_L1).toInteger();
    activity._fileAction = json.value("type"_L1).toString();
    activity._accName = account->displayName();
    activity._subject = json.value("subject"_L1).toString();
    activity._message = json.value("message"_L1).toString();
    activity._file = json.value("object_name"_L1).toString();
    activity._link = stringToUrl(account->url(), json.value("link"_L1).toString());
    activity._dateTime = QDateTime::fromString(json.value("datetime"_L1).toString(), Qt::ISODate);
    activity._icon = json.value("icon"_L1).toString();
    activity._isCurrentUserFileActivity = activity._objectType == "files"_L1 && activityUser == account->davUser();
    activity._isMultiObjectActivity = json.value("objects").toObject().count() > 1;

    auto richSubjectData = json.value("subject_rich"_L1).toArray();

    if(richSubjectData.size() > 1) {
        activity._subjectRich = richSubjectData[0].toString();
        auto parameters = richSubjectData[1].toObject();
        // keep the contents inside the {braces} in sync with server's \OCP\RichObjectStrings\IValidator::PLACEHOLDER_REGEX
        const QRegularExpression subjectRichParameterRe(uR"#(({[A-Za-z][A-Za-z0-9\-_.]+}))#"_s);
        const QRegularExpression subjectRichParameterBracesRe(u"[{}]"_s);

        for (auto i = parameters.begin(); i != parameters.end(); ++i) {
            const auto parameterJsonObject = i.value().toObject();

            const auto richParamLink = stringToUrl(account->url(), parameterJsonObject.value("link"_L1).toString());
            activity._subjectRichParameters[i.key()] = QVariant::fromValue(Activity::RichSubjectParameter{
                parameterJsonObject.value("type"_L1).toString(),
                parameterJsonObject.value("id"_L1).toString(),
                parameterJsonObject.value("name"_L1).toString(),
                parameterJsonObject.contains("path"_L1) ? parameterJsonObject.value("path"_L1).toString() : QString(),
                richParamLink,
            });

            if (activity._objectType == "calendar"_L1 && activity._link.isEmpty()) {
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

    const auto previewsData = json.value("previews"_L1).toArray();
    const QMimeDatabase mimeDb;

    for(const auto &preview : previewsData) {
        const auto jsonPreviewData = preview.toObject();

        PreviewData data;
        data._link = jsonPreviewData.value("link"_L1).toString();
        data._mimeType = jsonPreviewData.value("mimeType"_L1).toString();
        data._fileId = jsonPreviewData.value("fileId"_L1).toInt();
        data._view = jsonPreviewData.value("view"_L1).toString();
        data._filename = jsonPreviewData.value("filename"_L1).toString();

        const auto mimeType = mimeDb.mimeTypeForName(data._mimeType);

        if(data._mimeType.contains("text/"_L1) || data._mimeType.contains("/pdf"_L1)) {
            data._source = account->url().toString() + relativeServerFileTypeIconPath(mimeType);
            data._isMimeTypeIcon = true;
        } else {
            data._source = jsonPreviewData.value("source"_L1).toString();
            data._isMimeTypeIcon = jsonPreviewData.value("isMimeTypeIcon"_L1).toBool();
        }

        activity._previews.append(data);
    }

    if(!previewsData.isEmpty()) {
        if(activity._icon.contains("add-color.svg"_L1)) {
            activity._icon = "image://svgimage-custom-color/add.svg";
        } else if(activity._icon.contains("delete.svg"_L1)) {
            activity._icon = "image://svgimage-custom-color/delete.svg";
        } else if(activity._icon.contains("change.svg"_L1)) {
            activity._icon = "image://svgimage-custom-color/change.svg";
        }
    }

    auto actions = json.value("actions"_L1).toArray();
    for (const auto &action : std::as_const(actions)) {
        activity._links.append(ActivityLink::createFomJsonObject(action.toObject()));
    }

    return activity;
}

QString Activity::relativeServerFileTypeIconPath(const QMimeType &mimeType)
{
    const auto iconPath = QStringLiteral("/index.php/apps/theming/img/core/filetypes/");
    const auto defaultIcon = iconPath + QStringLiteral("file.svg");
    if (!mimeType.isValid()) {
        return defaultIcon;
    }

    if (mimeType.inherits("text/plain")) {
        return iconPath + QStringLiteral("text.svg");
    }

    if (mimeType.name().startsWith("image")) {
        return iconPath + QStringLiteral("image.svg");
    }

    if (mimeType.name().startsWith("audio")) {
        return iconPath + QStringLiteral("audio.svg");
    }

    if (mimeType.name().startsWith("video")) {
        return iconPath + QStringLiteral("video.svg");
    }

    const auto isDocument = mimeType.inherits("application/vnd.oasis.opendocument.text") ||
        mimeType.inherits("application/msword") ||
        mimeType.inherits("application/vnd.openxmlformats-officedocument.wordprocessingml.document") ||
        mimeType.inherits("application/vnd.openxmlformats-officedocument.wordprocessingml.template") ||
        mimeType.inherits("application/vnd.ms-word.document.macroEnabled.12")||
        mimeType.inherits("application/vnd.ms-word.template.macroEnabled.12") ||
        mimeType.inherits("application/vnd.apple.pages");
    if (isDocument) {
        return iconPath + QStringLiteral("x-office-document.svg");
    }

    if (mimeType.inherits("application/vnd.oasis.opendocument.graphics")) {
        return iconPath + QStringLiteral("x-office-drawing.svg");
    }

    const auto isPresentation = mimeType.inherits("application/vnd.oasis.opendocument.presentation") ||
        mimeType.inherits("application/vnd.ms-powerpoint") ||
        mimeType.inherits("application/vnd.openxmlformats-officedocument.presentationml.presentation") ||
        mimeType.inherits("application/vnd.openxmlformats-officedocument.presentationml.template") ||
        mimeType.inherits("application/vnd.openxmlformats-officedocument.presentationml.slideshow") ||
        mimeType.inherits("application/vnd.ms-powerpoint.addin.macroEnabled.12") ||
        mimeType.inherits("application/vnd.ms-powerpoint.presentation.macroEnabled.12") ||
        mimeType.inherits("application/vnd.ms-powerpoint.template.macroEnabled.12") ||
        mimeType.inherits("application/vnd.ms-powerpoint.slideshow.macroEnabled.12") ||
        mimeType.inherits("application/vnd.apple.keynote");
    if (isPresentation) {
        return iconPath + QStringLiteral("x-office-presentation.svg");
    }

    const auto isSpreadsheet = mimeType.inherits("application/vnd.oasis.opendocument.spreadsheet") ||
        mimeType.inherits("application/vnd.ms-excel") ||
        mimeType.inherits("application/vnd.openxmlformats-officedocument.spreadsheetml.sheet") ||
        mimeType.inherits("application/vnd.openxmlformats-officedocument.spreadsheetml.template") ||
        mimeType.inherits("application/vnd.ms-excel.sheet.macroEnabled.12") ||
        mimeType.inherits("application/vnd.ms-excel.template.macroEnabled.12") ||
        mimeType.inherits("application/vnd.ms-excel.addin.macroEnabled.12") ||
        mimeType.inherits("application/vnd.ms-excel.sheet.binary.macroEnabled.12") ||
        mimeType.inherits("application/vnd.apple.numbers");
    if (isSpreadsheet) {
        return iconPath + QStringLiteral("x-office-document.svg");
    }

    if (mimeType.inherits("application/pdf")) {
        return iconPath + QStringLiteral("application-pdf.svg");
    }

    return defaultIcon;
}

}
