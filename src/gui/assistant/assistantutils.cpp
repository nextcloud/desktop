/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "assistantutils.h"

#include <QDateTime>
#include <QJsonObject>
#include <QLocale>

using namespace Qt::StringLiterals;

namespace OCC::AssistantUtils {

qint64 jsonInteger(const QJsonValue &value, qint64 fallback)
{
    if (value.isDouble()) {
        return static_cast<qint64>(value.toDouble(fallback));
    }
    if (value.isString()) {
        auto ok = false;
        const auto result = value.toString().toLongLong(&ok);
        return ok ? result : fallback;
    }
    return fallback;
}

QString statusString(const QJsonValue &value)
{
    if (value.isString()) {
        return value.toString();
    }
    if (value.isDouble()) {
        return QString::number(static_cast<int>(value.toDouble()));
    }
    return {};
}

QString textFromValue(const QJsonValue &value)
{
    if (value.isString()) {
        return value.toString();
    }
    if (!value.isObject()) {
        return {};
    }

    const auto object = value.toObject();
    for (const auto &key : {u"input"_s, u"output"_s, u"text"_s, u"answer"_s, u"content"_s}) {
        const auto nestedValue = object.value(key);
        if (nestedValue.isString()) {
            return nestedValue.toString();
        }
        if (nestedValue.isObject()) {
            const auto nestedText = textFromValue(nestedValue);
            if (!nestedText.isEmpty()) {
                return nestedText;
            }
        }
    }
    return {};
}

QString dateText(qint64 timestamp)
{
    if (timestamp <= 0) {
        return {};
    }

    const auto dateTime = timestamp > 1000000000000LL
        ? QDateTime::fromMSecsSinceEpoch(timestamp)
        : QDateTime::fromSecsSinceEpoch(timestamp);
    return QLocale::system().toString(dateTime, QLocale::ShortFormat);
}

bool taskStillRunning(const QJsonValue &statusValue)
{
    const auto status = statusString(statusValue);
    return status != "3"_L1
        && status != "4"_L1
        && status != "STATUS_FAILED"_L1
        && status != "STATUS_SUCCESSFUL"_L1;
}

}
