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

ResolvedSetting ManagedSettings::resolve(const SettingDefinition &definition, const QString &group) const
{
    // Priorities encode the precedence: device policy > server enforced > user > server default > device default.
    const SettingSource *settingSource = nullptr;
    QVariant settingValue;
    auto settingPriority = -1;
    for (const auto &source : _sources) {
        if (source->enforcement() == EnforcementState::Enforced && !definition.enforceable) {
            continue;
        }
        auto value = source->read(definition.key, group);
        if (!value.has_value()) {
            continue;
        }
        if (definition.builtinDefault.isValid() && !value->convert(definition.builtinDefault.metaType())) {
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
        return {definition.key, definition.builtinDefault, SettingSourceType::BuiltinDefault, EnforcementState::NotEnforced, false};
    }

    return {definition.key, settingValue, settingSource->type(), settingSource->enforcement(), true};
}

QList<ResolvedSetting> ManagedSettings::resolveAll(const QList<SettingDefinition> &definitionsList, const QString &group) const
{
    QList<ResolvedSetting> results;
    results.reserve(definitionsList.size());
    for (const auto &definition : definitionsList) {
        results.append(resolve(definition, group));
    }
    return results;
}

} // namespace OCC
