/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef GOVERNANCETYPES_H
#define GOVERNANCETYPES_H

#include <QMetaObject>

namespace OCC {

namespace Governance {

Q_NAMESPACE

enum class EntityType {
    Files,
    Mails,
    Custom,
};

Q_ENUM_NS(EntityType)

enum class LabelType {
    Sensitivity,
    Retention,
    Hold,
    Invalid,
};

Q_ENUM_NS(LabelType)

enum class ApiVersion {
    Invalid,
    Version_1,
};

Q_ENUM_NS(ApiVersion)

}

}

#endif // GOVERNANCETYPES_H
