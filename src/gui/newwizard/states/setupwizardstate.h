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

#include <QObject>

namespace OCC::Wizard {
Q_NAMESPACE

/**
 * Determines the state the wizard is in. The number of states is equal to the number of pages in the pagination.
 *
 * Every state may be represented by more than one class. These classes can be used like different "strategies".
 * For instance, we have two different credentials state implementations representing different authentication
 * methods.
 *
 * Since we need to decide which states to have at compile time, disabling or hiding optional states must be taken
 * care of by the pagination class. All states have to be added here nevertheless.
 */
enum class SetupWizardState {
    ServerUrlState,
    CredentialsState,
    AccountConfiguredState,
};
Q_ENUM_NS(SetupWizardState)

}
