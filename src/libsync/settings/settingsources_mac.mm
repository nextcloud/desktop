/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "settings/settingsources.h"

#include <QString>
#include <QVariant>

#include <utility>

#include <CoreFoundation/CoreFoundation.h>

namespace OCC {

namespace {

QVariant toVariant(CFTypeRef value)
{
    if (!value) {
        return {};
    }
    const auto typeId = CFGetTypeID(value);
    if (typeId == CFBooleanGetTypeID()) {
        return QVariant(static_cast<bool>(CFBooleanGetValue(static_cast<CFBooleanRef>(value))));
    }
    if (typeId == CFStringGetTypeID()) {
        return QString::fromCFString(static_cast<CFStringRef>(value));
    }
    if (typeId == CFNumberGetTypeID()) {
        const auto number = static_cast<CFNumberRef>(value);
        if (CFNumberIsFloatType(number)) {
            double asDouble = 0;
            CFNumberGetValue(number, kCFNumberDoubleType, &asDouble);
            return asDouble;
        }
        qint64 asInt = 0;
        CFNumberGetValue(number, kCFNumberSInt64Type, &asInt);
        return QVariant::fromValue(asInt);
    }
    return {};
}

}

MacForcedPreferenceSource::MacForcedPreferenceSource(QString applicationId, int priority)
    : ForcedPreferenceSource(priority)
    , _applicationId(std::move(applicationId))
{
}

bool MacForcedPreferenceSource::isForced(const QString &key) const
{
    const auto appId = _applicationId.toCFString();
    const auto keyRef = key.toCFString();
    const auto forced = CFPreferencesAppValueIsForced(keyRef, appId);
    CFRelease(keyRef);
    CFRelease(appId);
    return forced;
}

std::optional<QVariant> MacForcedPreferenceSource::copyForcedValue(const QString &key) const
{
    const auto appId = _applicationId.toCFString();
    const auto keyRef = key.toCFString();
    const CFPropertyListRef value = CFPreferencesCopyAppValue(keyRef, appId);
    CFRelease(keyRef);
    CFRelease(appId);
    if (!value) {
        return std::nullopt;
    }
    const auto variant = toVariant(value);
    CFRelease(value);
    if (!variant.isValid()) {
        return std::nullopt;
    }
    return variant;
}

} // namespace OCC
