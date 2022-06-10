/*
 * Copyright (C) Fabian Müller <fmueller@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#pragma once

#include "common/utility.h"
#include "enums.h"

#include <QObject>

namespace OCC::Wizard {
Q_NAMESPACE

enum class SetupWizardState {
    ServerUrlState,
    FirstState = ServerUrlState,

    CredentialsState,

    AccountConfiguredState,
    FinalState = AccountConfiguredState,
};
Q_ENUM_NS(SetupWizardState)

enum class SyncMode {
    Invalid = 0,
    SyncEverything,
    ConfigureUsingFolderWizard,
    UseVfs,
};
Q_ENUM_NS(SyncMode)

}

namespace OCC {
template <>
QString Utility::enumToDisplayName(Wizard::SetupWizardState state);
}
