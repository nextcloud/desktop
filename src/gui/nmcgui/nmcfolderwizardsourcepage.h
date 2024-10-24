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

#ifndef MIRALL_NMCFolderWizardSourcePage_H
#define MIRALL_NMCFolderWizardSourcePage_H

#include "ui_folderwizardsourcepage.h"

/**
 * @brief The NMCFolderWizardSourcePage class represents a source page for the NMCFolderWizard.
 * @ingroup gui
 *
 * The NMCFolderWizardSourcePage class is derived from the Ui::FolderWizardSourcePage class and provides
 * additional functionality specific to the NMCFolderWizard in the NMC application (replace with the actual application name).
 */

namespace OCC {

class NMCFolderWizardSourcePage : public ::Ui::FolderWizardSourcePage
{

public:
    /**
     * @brief Constructs an instance of NMCFolderWizardSourcePage.
     */
    explicit NMCFolderWizardSourcePage();

    /**
     * @brief Destroys the NMCFolderWizardSourcePage instance.
     */
    ~NMCFolderWizardSourcePage() = default;

    /**
     * @brief Sets default settings for the NMCFolderWizardSourcePage.
     */
    void setDefaultSettings();

    /**
     * @brief Changes the layout of the NMCFolderWizardSourcePage.
     */
    void changeLayout();
};

} // namespace OCC

#endif
