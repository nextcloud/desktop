/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "settings/managedsettings.h"

namespace OCC {

SettingSource::~SettingSource() = default;

void ManagedSettings::addSource(std::unique_ptr<SettingSource> source)
{
    _sources.push_back(std::move(source));
}

ManagedValue ManagedSettings::resolve(const SettingSpec &spec, const QString &group) const
{
    // Priorities encode the precedence: device policy > server enforced > user > server default > device default.
    const SettingSource *settingSource = nullptr;
    QVariant settingValue;
    auto settingPriority = -1;
    for (const auto &source : _sources) {
        if (source->enforcement() == EnforcementState::Enforced && !spec.enforceable) {
            continue;
        }
        const auto value = source->read(spec.key, group);
        if (!value.has_value()) {
            continue;
        }
        const auto priority = source->priority();
        if (priority > settingPriority) {
            settingSource = source.get();
            settingValue = *value;
            settingPriority = priority;
        }
    }

    if (!settingSource) {
        return {spec.key, spec.builtinDefault, SettingSourceType::BuiltinDefault, EnforcementState::NotEnforced, false};
    }

    if (spec.builtinDefault.isValid()) {
        settingValue.convert(spec.builtinDefault.metaType());
    }

    return {spec.key, settingValue, settingSource->type(), settingSource->enforcement(), true};
}

QList<ManagedValue> ManagedSettings::resolveAll(const QList<SettingSpec> &specs, const QString &group) const
{
    QList<ManagedValue> results;
    results.reserve(specs.size());
    for (const auto &spec : specs) {
        results.append(resolve(spec, group));
    }
    return results;
}

} // namespace OCC
