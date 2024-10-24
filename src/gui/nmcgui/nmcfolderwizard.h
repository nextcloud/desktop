/*
 * Copyright (C) by Eugen Fischer
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

#ifndef MIRALL_NMCFolderWizard_H
#define MIRALL_NMCFolderWizard_H

#include "folderwizard.h"

/**
 * @brief The NMCFolderWizard class represents a specific folder wizard for the NMC application.
 * @ingroup gui
 *
 * The NMCFolderWizard class is derived from the FolderWizard class and provides additional functionality
 * specific to the NMC (replace with the actual application name) application.
 */

namespace OCC {

class NMCFolderWizard : public FolderWizard
{
    Q_OBJECT
public:
    /**
     * @brief Constructs an instance of NMCFolderWizard.
     * @param account The account associated with the wizard.
     * @param parent The parent widget (default is nullptr).
     */
    explicit NMCFolderWizard(OCC::AccountPtr account, QWidget *parent = nullptr);

    /**
     * @brief Destroys the NMCFolderWizard instance.
     */
    ~NMCFolderWizard() = default;
};

} // namespace OCC

#endif

