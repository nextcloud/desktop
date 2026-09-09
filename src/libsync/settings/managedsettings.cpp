/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "settings/managedsettings.h"

namespace OCC {

namespace {
// Higher tier always wins; within a tier the higher source priority wins.
int tierOf(const EnforcementState enforcement, const SettingSourceKind kind)
{
    switch (enforcement) {
    case EnforcementState::Enforced:
        return 2;
    case EnforcementState::NotEnforced:
        return kind == SettingSourceKind::UserConfig ? 1 : 0;
    }
    return 0;
}
}

SettingSource::~SettingSource() = default;

void ManagedSettings::addSource(std::unique_ptr<SettingSource> source)
{
    _sources.push_back(std::move(source));
}

ManagedValue ManagedSettings::resolve(const SettingSpec &spec, const QString &group) const
{
    const SettingSource *winner = nullptr;
    QVariant winnerValue;
    auto winnerTier = -1;
    auto winnerPriority = 0;

    for (const auto &source : _sources) {
        const auto enforcement = source->enforcement();
        if (enforcement == EnforcementState::Enforced && !spec.enforceable) {
            continue;
        }
        const auto value = source->read(spec.key, group);
        if (!value.has_value()) {
            continue;
        }
        const auto tier = tierOf(enforcement, source->kind());
        const auto priority = source->priority();
        if (tier > winnerTier || (tier == winnerTier && priority > winnerPriority)) {
            winner = source.get();
            winnerValue = *value;
            winnerTier = tier;
            winnerPriority = priority;
        }
    }

    if (!winner) {
        return {spec.key, spec.builtinDefault, SettingSourceKind::BuiltinDefault, EnforcementState::NotEnforced, false};
    }
    // Convert to the schema declared type, which builtinDefault carries.
    if (spec.builtinDefault.isValid()) {
        winnerValue.convert(spec.builtinDefault.metaType());
    }
    return {spec.key, winnerValue, winner->kind(), winner->enforcement(), true};
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
