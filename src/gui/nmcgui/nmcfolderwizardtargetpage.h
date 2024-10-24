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

#ifndef MIRALL_NMCFolderWizardTargetPage_H
#define MIRALL_NMCFolderWizardTargetPage_H

#include "ui_folderwizardtargetpage.h"

/**
 * @brief The NMCFolderWizardTargetPage class represents a target page for the NMCFolderWizard.
 * @ingroup gui
 *
 * The NMCFolderWizardTargetPage class is derived from the Ui::FolderWizardTargetPage class and provides
 * additional functionality specific to the NMCFolderWizard in the NMC application (replace with the actual application name).
 */

namespace OCC {

class NMCFolderWizardTargetPage : public ::Ui::FolderWizardTargetPage
{

public:
    /**
     * @brief Constructs an instance of NMCFolderWizardTargetPage.
     */
    explicit NMCFolderWizardTargetPage();

    /**
     * @brief Destroys the NMCFolderWizardTargetPage instance.
     */
    ~NMCFolderWizardTargetPage() = default;

    /**
     * @brief Sets default settings for the NMCFolderWizardTargetPage.
     */
    void setDefaultSettings();

    /**
     * @brief Sets the layout for the NMCFolderWizardTargetPage.
     */
    void setLayout();
};

} // namespace OCC

#endif
