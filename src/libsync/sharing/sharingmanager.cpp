/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "sharingmanager.h"

#include <algorithm>
#include <QSet>

using namespace Qt::StringLiterals;

using namespace OCC::Sharing;

namespace
{
template<class T>
void parseTypes(const QVariantMap &capability, const QString &key, QMap<QString, QSharedPointer<T>> &typeMap) {
    const auto propertyList = capability.value(key).toList();

    typeMap.clear();
    for (const auto &typeObject  : propertyList) {
        const auto object = typeObject.toMap();
        auto typeInstance = T::fromCapability(object);
        if (!typeInstance) {
            continue;
        }

        typeMap.insert(typeInstance->type(), typeInstance);
    }
};
}

SharingManager::SharingManager(AccountPtr account, QObject *parent)
    : QObject{parent}
    , _account{account}
{

}

void SharingManager::updateFromCapabilities(const QVariantMap &capabilities)
{
    if (const auto property = capabilities.value("sharing"_L1); !property.isValid()) {
        // no sharing capability present, we're done here
        setAvailable(false);
        return;
    }
    setAvailable(true);

    const auto capability = capabilities.value("sharing"_L1).toMap();

    if (const auto property = capability.value("api_versions"_L1); property.isValid() && property.canConvert<QStringList>()) {
        _apiVersions = property.value<QStringList>();
    }

    parseTypes(capability, "source_types"_L1, _sourceTypes);
    parseTypes(capability, "recipient_types"_L1, _recipientTypes);
    parseTypes(capability, "features"_L1, _features);
}

QList<QSharedPointer<Feature>> SharingManager::availableFeatures(const QStringList &sourceTypes, const QStringList &recipientTypes) const
{
    QList<QSharedPointer<Feature>> features;

    const auto sourceTypesSet = QSet(sourceTypes.cbegin(), sourceTypes.cend());
    const auto recipientTypesSet = QSet(recipientTypes.cbegin(), recipientTypes.cend());

    for (const auto &feature : std::as_const(_features)) {
        if (const auto compatibleSourceTypes = feature->compatibleSourceTypes();
            !QSet(compatibleSourceTypes.cbegin(), compatibleSourceTypes.cend()).intersects(sourceTypesSet)) {
            continue;
        }
        if (const auto compatibleRecipientTypes = feature->compatibleRecipientTypes();
            !QSet(compatibleRecipientTypes.cbegin(), compatibleRecipientTypes.cend()).intersects(sourceTypesSet)) {
            continue;
        }

        features.append(feature);
    }

    return features;
}

bool SharingManager::isFeatureAvailable(const QString &feature, const QStringList &sourceTypes, const QStringList &recipientTypes) const
{
    if (!_features.contains(feature)) {
        return false;
    }
    const auto featureObject = _features.value(feature);

    const auto sourceTypesSet = QSet(sourceTypes.cbegin(), sourceTypes.cend());
    const auto recipientTypesSet = QSet(recipientTypes.cbegin(), recipientTypes.cend());

    if (const auto compatibleSourceTypes = featureObject->compatibleSourceTypes();
        !QSet(compatibleSourceTypes.cbegin(), compatibleSourceTypes.cend()).intersects(sourceTypesSet)) {
        return false;
    }

    if (const auto compatibleRecipientTypes = featureObject->compatibleRecipientTypes();
        !QSet(compatibleRecipientTypes.cbegin(), compatibleRecipientTypes.cend()).intersects(sourceTypesSet)) {
        return false;
    }

    return true;
}

bool SharingManager::isAvailable() const
{
    return _available;
}

void SharingManager::setAvailable(bool available)
{
    if (_available == available) {
        return;
    }

    _available = available;
    Q_EMIT availableChanged();
}

QMap<QString, QSharedPointer<ShareType>> SharingManager::sourceTypes() const
{
    return _sourceTypes;
}

QMap<QString, QSharedPointer<ShareType>> SharingManager::recipientTypes() const
{
    return _recipientTypes;
}

QMap<QString, QSharedPointer<Feature>> SharingManager::features() const
{
    return _features;
}
