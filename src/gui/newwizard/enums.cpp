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

// just a stub so the MOC file can be included somewhere
#include "moc_enums.cpp"

#include <QApplication>

using namespace OCC::Wizard;

namespace {
const char contextC[] = "SetupWizardState";
}

template <>
QString OCC::Utility::enumToDisplayName(SetupWizardState state)
{
    switch (state) {
    case SetupWizardState::ServerUrlState:
        return QApplication::translate(contextC, "Server URL");
    case SetupWizardState::CredentialsState:
        return QApplication::translate(contextC, "Credentials");
    case SetupWizardState::AccountConfiguredState:
        return QApplication::translate(contextC, "Sync Options");
    default:
        Q_UNREACHABLE();
    }
}
