/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QVariantMap>
#include <QStringList>

namespace OCC::CapabilityInfo {

/**
 * Capability information from the `sharing` app (bundled since Nextcloud server 34)
 */
class Sharing
{
public:
    using ShareType = QString;

    Sharing(const QVariantMap &capabilities);

    [[nodiscard]] bool isAvailable() const;

    [[nodiscard]] QStringList apiVersions() const;

    [[nodiscard]] QList<ShareType> sourceTypes() const;
    [[nodiscard]] QList<ShareType> recipientTypes() const;

private:
    using ShareTypeMap = QMap<ShareType, QString>;

    bool _available = false;
    QStringList _apiVersions;
    ShareTypeMap _sourceTypes;
    ShareTypeMap _recipientTypes;
};

}
