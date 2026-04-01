/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "sharing.h"

using namespace Qt::StringLiterals;

namespace OCC::CapabilityInfo {

Sharing::Sharing(const QVariantMap &capabilities)
{
    if (const auto property = capabilities.value("sharing"_L1); !property.isValid()) {
        // no sharing capability present, we're done here
        _available = false;
        return;
    }
    _available = true;

    const auto capability = capabilities.value("sharing"_L1).toMap();

    if (const auto property = capability.value("api_versions"_L1); property.isValid() && property.canConvert<QStringList>()) {
        _apiVersions = property.value<QStringList>();
    }

    const auto loadTypeMap = [&capability](const QString &key, ShareTypeMap &typeMap) -> void {
        if (const auto property = capability.value(key); property.isValid()) {
            const auto propertyVariantMap = property.toMap();
            for (const auto [typeName, displayName] : propertyVariantMap.asKeyValueRange()) {
                if (!displayName.canConvert<QString>()) {
                    // not a display name
                    continue;
                }

                typeMap.insert(typeName, displayName.toString());
            }
        }
    };

    loadTypeMap("source_types"_L1, _sourceTypes);
    loadTypeMap("recipient_types"_L1, _recipientTypes);

    // TODO: detect "features" and deal with compatible_*
}

bool Sharing::isAvailable() const
{
    return _available;
}

QStringList Sharing::apiVersions() const
{
    return _apiVersions;
}

QList<Sharing::ShareType> Sharing::sourceTypes() const
{
    return _sourceTypes.keys();
}

QList<Sharing::ShareType> Sharing::recipientTypes() const
{
    return _recipientTypes.keys();
}

}
