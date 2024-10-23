/*
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
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

#ifndef MIRALL_NETWORKSETTINGSMAGENTA_H
#define MIRALL_NETWORKSETTINGSMAGENTA_H

#include "networksettings.h"

namespace OCC {

/**
 * @brief The NMCNetworkSettings class
 * @ingroup gui
 * @brief Derived class for network settings specific to NMC (Magenta) in the ownCloud client.
 */
class NMCNetworkSettings : public NetworkSettings
{
    Q_OBJECT

public:
    /**
     * @brief Constructs an NMCNetworkSettings object.
     * @param parent Pointer to the parent QWidget.
     */
    explicit NMCNetworkSettings(QWidget *parent = nullptr);

    /**
     * @brief Destructor for NMCNetworkSettings.
     */
    ~NMCNetworkSettings() = default;

private:
    /**
     * @brief Sets the layout for the network settings specific to NMC (Magenta).
     */
    void setLayout();
};

} // namespace OCC
#endif // MIRALL_NETWORKSETTINGSMAGENTA_H